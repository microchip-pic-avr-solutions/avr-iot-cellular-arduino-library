from pathlib import Path
import re
import argparse


def get_path_relative_to_root(root_path: Path, file_path: Path):
    """Returns the relative path of the file path to the root 
       path, appended with the root path.

    Args:
        root_path (Path): Root path directory
        file_path (Path): File path in the root directory

    Returns:
        Path: The full path to the file relative to the root path 
    """
    return Path((root_path / file_path.relative_to(root_path)).as_posix())


def get_include_paths_relative_to_root(root_path: Path):
    """Fetches every .h file in a directory to build a list of include paths.

    Args:
        root_path (Path): The root path of the directory

    Returns:
        list: List of paths to the include files
    """

    include_paths = []

    for path in root_path.rglob("*.h"):
        include_paths.append(get_path_relative_to_root(root_path, path))

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


def get_update_line(line: str, include_paths_relative_to_root):
    """Given a line in a file, replaces a #include <...> or #include "..." with
       a path relative to the root folder given in the include paths relative 
       to root list. 

    Args:
        line (str): The line in the file 
        include_paths_relative_to_root (list[Path]): List of the include paths 
                                                     relative to the root.

    Returns:
        str: The updated line if "#include" was found.
    """

    # Find includes with <> or ""
    r = re.search('["<](.*)[">]', line)

    if r is not None and r.group(1).endswith(".h"):
        include_path = r.group(1)

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

    args = parser.parse_args()

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
                    updated_line = get_update_line(line, include_paths_relative_to_root)

                    if args.verbose:
                        if line != updated_line:
                            print(f"\tChanged {line.strip()} to {updated_line.strip()}")

                    output_file.write(updated_line)
