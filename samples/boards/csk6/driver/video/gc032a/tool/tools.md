# Tools manual

## install dependencies

```shell
pip install -r ./requirements.txt
```

### Note: Recheck and generate dependency list

> Use pipreqs to generate a list of dependencies

```shell
pip install pipreqs
```

```shell
pipreqs ./ --force
```

## User

> Make sure it is consistent with the serial port baud rate configured in the device tree

```shell
python3 ./show_image.py
# Or customize the device and baud rate
python3 ./show_image.y -d /dev/ttyACM0 -d b 921600
```

- Help

    ```shell
    ➜  tool git:(main) ✗ python3 ./show_image.py -h
    show_image.py -d <serail_prot device> -b <baud_rate>
    default Config SeailPort:/dev/ttyACM0, Baudrate:921600
    ```
