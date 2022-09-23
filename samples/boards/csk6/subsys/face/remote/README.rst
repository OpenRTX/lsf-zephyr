目前资源放在flash上,烧录地址在0x18100000.

烧录的时候可以将所有资源打包为一个资源进行烧录。

cat face_detect.bin face_align.bin anti_spoofing.bin face_verify.bin > face_res.bin

pyocd flash -e sector -a 0x18000000 -t csk6001 --base-address=0x18100000 face_res.bin --frequency=30000000