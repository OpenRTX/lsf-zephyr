#ifndef __LISA_EVS_CONFIG_H__
#define __LISA_EVS_CONFIG_H__

//client_id，iflyos平台分配
//https://device.iflyos.cn/product/device-info
//从设备控制台=>产品信息=>client_id 获取
#define PRODUCT_CLIENT_ID ("ad9f3c5d-38fd-4383-8d8d-8d460590de22")

//正式集成的时候需要从设备中获取，不同的设备deviceid不一样，不能重复！！！
//https://device.iflyos.cn/product/device-id
//从上述链接导入设备白名单，并且确保每台设备device_id不一样，否则产生设备竞争，导致设备和账号异常。
#define PRODUCT_DEVICE_ID ("12345")

//ota_secret_id，iflyos平台分配
//https://device.iflyos.cn/product/auto-update 
//从设备控制台=>自动更新=>加密密钥 获取
#define PRODUCT_OTA_SECRET_ID ("7c7e25414ec0389d6a3d68645d93fea804aa972b")

#endif // __LISA_EVS_CONFIG_H__
