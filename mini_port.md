# UART

## CDC

Change from `Serial5` to `Serial3`.

| Board         | TX  | RX  |
| ------------- | --- | --- |
| Cellular      | PG0 | PG1 |
| Cellular Mini | PB0 | PB1 |

## Sequans

| Board         | TX  | RX  | RING | CTS | RTS | RESETN |
| ------------- | --- | --- | ---- | --- | --- | ------ |
| Cellular      | PC0 | PC1 | PC4  | PC6 | PC7 | PE1    |
| Cellular Mini | PC0 | PC1 | PC6  | PC4 | PC7 | PC5    |


# I2C

## ECC

Do we need to rebuild cryptoauthlib for the 48 pin variant?

| Board         | SDA | SCL | Instance |
| ------------- | --- | --- | -------- |
| Cellular      | PB2 | PB3 | 1        |
| Cellular Mini | Pc2 | PC3 | 0        |

## Sensor Bus

| Board         | SDA | SCL | Instance |
| ------------- | --- | --- | -------- |
| Cellular      | PC2 | PC3 | 0        |
| Cellular Mini | PF2 | PF3 | 1        |