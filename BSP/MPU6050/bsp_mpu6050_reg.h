#pragma once
#ifndef BSP_MPU6050_REG_H
#define BSP_MPU6050_REG_H

#define MPU_SELF_TESTX_REG              0x0D  // 自检寄存器X
#define MPU_SELF_TESTY_REG              0x0E  // 自检寄存器Y
#define MPU_SELF_TESTZ_REG              0x0F  // 自检寄存器Z
#define MPU_SELF_TESTA_REG              0x10  // 自检寄存器A
#define MPU_SAMPLE_RATE_REG             0x19  // 采样频率分频器
#define MPU_CFG_REG                     0x1A  // 配置寄存器
#define MPU_GYRO_CFG_REG                0x1B  // 陀螺仪配置寄存器
#define MPU_ACCEL_CFG_REG               0x1C  // 加速度计配置寄存器
#define MPU_MOTION_DET_REG              0x1F  // 运动检测阈值设置寄存器
// #define MPU_MOTION_DUR_REG              0x20  // 运动检测时间寄存器
#define MPU_FIFO_EN_REG                 0x23  // FIFO寄存器使能
#define MPU_I2CMST_CTRL_REG             0x24  // I2C主机控制寄存器
#define MPU_I2CSLV0_ADDR_REG            0x25  // IIC从机0器件地址寄存器
#define MPU_I2CSLV0_REG                 0x26  // IIC从机0数据寄存器
#define MPU_I2CSLV0_CTRL_REG            0X27  // IIC从机0控制寄存器
#define MPU_I2CSLV1_ADDR_REG            0x28  // IIC从机1器件地址寄存器
#define MPU_I2CSLV1_REG                 0x29  // IIC从机1数据寄存器
#define MPU_I2CSLV1_CTRL_REG            0X2A  // IIC从机1控制寄存器
#define MPU_I2CSLV2_ADDR_REG            0x2B  // IIC从机2器件地址寄存器
#define MPU_I2CSLV2_REG                 0x2C  // IIC从机2数据寄存器
#define MPU_I2CSLV2_CTRL_REG            0X2D  // IIC从机2控制寄存器
#define MPU_I2CSLV3_ADDR_REG            0x2E  // IIC从机0器件地址寄存器
#define MPU_I2CSLV3_REG                 0x2F  // IIC从机0数据寄存器
#define MPU_I2CSLV3_CTRL_REG            0X30  // IIC从机0控制寄存器
#define MPU_I2CSLV4_ADDR_REG            0x31  // IIC从机4器件地址寄存器
#define MPU_I2CSLV4_REG                 0x32  // IIC从机4数据寄存器
#define MPU_I2CSLV4_DO_REG              0X33  // IIC从机4写数据寄存器
#define MPU_I2CSLV4_CTRL_REG            0X34  // IIC从机4控制寄存器
#define MPU_I2CSLV4_DI_REG              0X35  // IIC从机4读数据寄存器

#define MPU_I2CMST_STA_REG              0x36  // IIC主机状态寄存器
#define MPU_INTBP_CFG_REG               0x37  // 中断引脚\旁路配置寄存器
#define MPU_INT_EN_REG                  0x38  // 中断使能寄存器
#define MPU_INT_STA_REG                 0x3A  // 中断状态寄存器

#define MPU_ACCEL_XOUTH_REG             0x3B  // 加速度值，X轴高8位寄存器
#define MPU_ACCEL_XOUTL_REG             0x3C  // 加速度值，X轴低8位寄存器
#define MPU_ACCEL_YOUTH_REG             0x3D  // 加速度值，Y轴高8位寄存器
#define MPU_ACCEL_YOUTL_REG             0x3E  // 加速度值，Y轴低8位寄存器
#define MPU_ACCEL_ZOUTH_REG             0x3F  // 加速度值，Z轴高8位寄存器
#define MPU_ACCEL_ZOUTL_REG             0x40  // 加速度值，Z轴低8位寄存器

#define MPU_TEMP_OUTH_REG               0x41  // 温度值，高8位寄存器
#define MPU_TEMP_OUTL_REG               0x42  // 温度值，低8位寄存器

#define MPU_GYRO_XOUTH_REG              0x43  // 陀螺仪值，X轴高8位寄存器
#define MPU_GYRO_XOUTL_REG              0x44  // 陀螺仪值，X轴低8位寄存器
#define MPU_GYRO_YOUTH_REG              0x45  // 陀螺仪值，Y轴高8位寄存器
#define MPU_GYRO_YOUTL_REG              0x46  // 陀螺仪值，Y轴低8位寄存器
#define MPU_GYRO_ZOUTH_REG              0x47  // 陀螺仪值，Z轴高8位寄存器
#define MPU_GYRO_ZOUTL_REG              0x48  // 陀螺仪值，Z轴低8位寄存器

#define MPU_EXT_SENS_DATA_00_REG        0x49  // 扩展传感器数据寄存器
#define MPU_EXT_SENS_DATA_01_REG        0x4A  // 扩展传感器数据寄存器
#define MPU_EXT_SENS_DATA_02_REG        0x4B  // 扩展传感器数据寄存器
#define MPU_EXT_SENS_DATA_03_REG        0x4C  // 扩展传感器数据寄存器
#define MPU_EXT_SENS_DATA_04_REG        0x4D  // 扩展传感器数据寄存器
#define MPU_EXT_SENS_DATA_05_REG        0x4E  // 扩展传感器数据寄存器
#define MPU_EXT_SENS_DATA_06_REG        0x4F  // 扩展传感器数据寄存器
#define MPU_EXT_SENS_DATA_07_REG        0x50  // 扩展传感器数据寄存器
#define MPU_EXT_SENS_DATA_08_REG        0x51  // 扩展传感器数据寄存器
#define MPU_EXT_SENS_DATA_09_REG        0x52  // 扩展传感器数据寄存器
#define MPU_EXT_SENS_DATA_10_REG        0x53  // 扩展传感器数据寄存器
#define MPU_EXT_SENS_DATA_11_REG        0x54  // 扩展传感器数据寄存器
#define MPU_EXT_SENS_DATA_12_REG        0x55  // 扩展传感器数据寄存器
#define MPU_EXT_SENS_DATA_13_REG        0x56  // 扩展传感器数据寄存器
#define MPU_EXT_SENS_DATA_14_REG        0x57  // 扩展传感器数据寄存器
#define MPU_EXT_SENS_DATA_15_REG        0x58  // 扩展传感器数据寄存器
#define MPU_EXT_SENS_DATA_16_REG        0x59  // 扩展传感器数据寄存器
#define MPU_EXT_SENS_DATA_17_REG        0x5A  // 扩展传感器数据寄存器
#define MPU_EXT_SENS_DATA_18_REG        0x5B  // 扩展传感器数据寄存器
#define MPU_EXT_SENS_DATA_19_REG        0x5C  // 扩展传感器数据寄存器
#define MPU_EXT_SENS_DATA_20_REG        0x5D  // 扩展传感器数据寄存器
#define MPU_EXT_SENS_DATA_21_REG        0x5E  // 扩展传感器数据寄存器
#define MPU_EXT_SENS_DATA_22_REG        0x5F  // 扩展传感器数据寄存器
#define MPU_EXT_SENS_DATA_23_REG        0x60  // 扩展传感器数据寄存器

#define MPU_I2CSLV0_DO_REG              0x63  // IIC从机0数据寄存器
#define MPU_I2CSLV1_DO_REG              0x64  // IIC从机1数据寄存器
#define MPU_I2CSLV2_DO_REG              0x65  // IIC从机2数据寄存器
#define MPU_I2CSLV3_DO_REG              0x66  // IIC从机3数据寄存器

#define MPU_I2CMST_DELAY_REG            0x67  // IIC主机延时管理寄存器
#define MPU_SIGPATH_RST_REG             0x68  // 信号通道复位寄存器
#define MPU_MDETECT_CTRL_REG            0x69  // 运动检测控制寄存器
#define MPU_USER_CTRL_REG               0x6A  // 用户控制寄存器
#define MPU_PWR_MGMT_1_REG              0x6B  // 电源管理1寄存器
#define MPU_PWR_MGMT_2_REG              0x6C  // 电源管理2寄存器
#define MPU_FIFO_CNTH_REG               0x72  // FIFO计数寄存器高8位
#define MPU_FIFO_CNTL_REG               0x73  // FIFO计数寄存器低8位
#define MPU_FIFO_RW_REG                 0x74  // FIFO读写寄存器
#define MPU_DEVICE_ID_REG               0x75  // 设备ID寄存器

// 如果AD0引脚（9脚）接地，IIC地址为0x68（不包含最低位）
// 如果接V3.3， 则IIC地址为0x69（不包含最低位）
#define MPU_ADDR                        0x68   
#define MPU_ID                          0x68

#endif /* end of BSP_MPU6050_REG_H */
