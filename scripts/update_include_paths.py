"""
This file replaces the include paths of a C/C++ project with the full path up to
the root of the project. This script exists due to not being able to add include
search paths in Arduino, so for e.g. cryptoauthlib, we need to modify all the
include paths to be relative to the src folder (which is the only search include 
path in Arduino).
"""

from pathlib import Path
import re
import argparse


def get_include_paths_relative_to_root(root_path: Path):
    """Fetches every .h file in a directory to build a list of include paths.

    Args:
        root_path (Path): The root path of the directory

    Returns:
        list: List of paths to the include files
    """

    include_paths = []

    for path in root_path.rglob("*.h"):
        include_paths.append(Path((Path(root_path.name) / path.relative_to(root_path)).as_posix()))

    return include_paths


def find_matching_root_include_path(include_path: str, include_paths_relative_to_root):
    """Given a include path, finds the corresponding include path relative to the root.

    Args:
        include_path (str): The include path in the file.
        include_paths (list[Path]): The list of include paths which are 
                                    relative to the root directory.

    Returns:
        Path: The matched path relative to root, or None if not found
    """

    for include_path_relative_to_root in include_paths_relative_to_root:

        if Path(include_path).name == include_path_relative_to_root.name:
            return include_path_relative_to_root

    return None


def get_update_line(line: str, include_paths_relative_to_root, source_overrides, destination_overrides):
    """Given a line in a file, replaces a #include <...> or #include "..." with
       a path relative to the root folder given in the include paths relative 
       to root list. If the item found is in the source overrides, it will be 
       replaced with the corresponding item in destination overrides

    Args:
        line (str): The line in the file 
        include_paths_relative_to_root (list[Path]): List of the include paths 
                                                     relative to the root.
        source_overrides (list[str]): List of source include path overrides.
        destination_overrides (list[str]): List of destination include path overrides. 

    Returns:
        str: The updated line if "#include" was found.
    """

    # Find includes with <> or ""
    r = re.search('["<](.*)[">]', line)

    if r is not None and r.group(1).endswith(".h"):
        include_path = r.group(1)

        if include_path in source_overrides:
            index = source_overrides.index(include_path)
            return line.replace(include_path, destination_overrides[index])
        else:
            include_path_relative_to_root = find_matching_root_include_path(
                include_path,
                include_paths_relative_to_root
            )

            # If the match was not found, the include is likely a system header
            if include_path_relative_to_root is not None:
                return line.replace(include_path, include_path_relative_to_root.as_posix())

    return line


if __name__ == "__main__":

    parser = argparse.ArgumentParser(
        prog="update_include_paths",
        description="Updates the include paths of a library so that all are relative to a root folder",
    )

    parser.add_argument("input_directory")
    parser.add_argument("output_directory")
    parser.add_argument("-v", "--verbose", action="store_true")
    parser.add_argument("-s", "--source-overrides", action="append",
                        help="List of source overrides, the source and destination needs to be passed in pairs with the -s and -d flags. This overrides e.g. foo.h with bar/foo.h if -s foo.h and -d bar/foo.h are passed", default=[])
    parser.add_argument("-d", "--destination-overrides", action="append",
                        help="List of destination overrides, the source and destination needs to be passed in pairs with the -s and -d flags. This overrides e.g. foo.h with bar/foo.h if -s foo.h and -d bar/foo.h are passed", default=[])

    args = parser.parse_args()

    if len(args.source_overrides) != len(args.destination_overrides):
        print("Error, source and destination overrides need to be passed in pairs when using the -s and -d flags")

    root_path = Path(args.input_directory)
    output_root_path = Path(args.output_directory)

    # Fetch every .h file to build a reference for the include paths
    include_paths_relative_to_root = get_include_paths_relative_to_root(root_path)

    # Find every .h and .c file
    paths = []

    for path in root_path.rglob("*.c"):
        paths.append(path)

    for path in root_path.rglob("*.h"):
        paths.append(path)

    # Loop through every file and update includes so that every include is
    # relative (and including) the root path
    for path in paths:

        if args.verbose:
            print(f"Opening {path}")

        with open(path) as input_file:

            # We need to remove the directories leading to the input directory
            output_path = output_root_path / path.relative_to(Path(args.input_directory))
            output_path.parent.mkdir(exist_ok=True, parents=True)

            with open(output_path, "w") as output_file:

                for line in input_file:
                    updated_line = get_update_line(line, include_paths_relative_to_root,
                                                   args.source_overrides, args.destination_overrides)

                    if args.verbose:
                        if line != updated_line:
                            print(f"\tChanged {line.strip()} to {updated_line.strip()}")

                    output_file.write(updated_line)
