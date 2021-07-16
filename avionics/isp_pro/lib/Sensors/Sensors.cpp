#include "Sensors.h"

IMU::IMU()
#ifdef USE_PERIPHERAL_MPU6050
    : mpu(MPU6050_ADDRESS_AD0_LOW)
#endif
{
    pose = ROCKET_UNKNOWN;
    imu_update_flag = 0;
}

ERROR_CODE IMU::init()
{
#ifdef USE_PERIPHERAL_MPU6050
// join I2C bus (I2Cdev library doesn't do this automatically)
#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
    Wire.begin();
    Wire.setClock(400000);  // 400kHz I2C clock. Comment this line if having
                            // compilation difficulties
#elif I2CDEV_IMPLEMENTATION == I2CDEV_BUILTIN_FASTWIRE
    Fastwire::setup(400, true);
#endif

    mpu.initialize();
    // pinMode(PIN_IMU_INT, INPUT);
    delay(100);

    if (!mpu.testConnection())
        return ERROR_MPU_INIT_FAILED;

    // load and configure the DMP
    // accel scale default to +/- 2g
    devStatus = mpu.dmpInitialize();

    // mpu.setRate();
    // mpu.setDLPFMode();
    // mpu.setFullScaleGyroRange(MPU6050_GYRO_FS_1000);
    // mpu.setFullScaleAccelRange(MPU6050_ACCEL_FS_16);
    // mpu.setDHPFMode();

    // supply your own gyro offsets here, scaled for min sensitivity
    mpu.setXGyroOffset(57);
    mpu.setYGyroOffset(2);
    mpu.setZGyroOffset(1);
    // mpu.setXAccelOffset(-2424);
    // mpu.setYAccelOffset(879);
    mpu.setZAccelOffset(1064);

    // make sure it worked (returns 0 if so)
    if (devStatus == 0) {
        // Calibration Time: generate offsets and calibrate our MPU6050
        mpu.CalibrateAccel(6);
        mpu.CalibrateGyro(6);
        mpu.PrintActiveOffsets();

        // turn on the DMP, now that it's ready
        mpu.setDMPEnabled(true);

        // enable Arduino interrupt detection
        // attachInterrupt(digitalPinToInterrupt(PIN_IMU_INT), dmpDataReady,
        //                RISING);
        mpuIntStatus = mpu.getIntStatus();

        // set our DMP Ready flag so the main loop() function knows it's okay to
        // use it
        dmpReady = true;

        // get expected DMP packet size for later comparison
        packetSize = mpu.dmpGetFIFOPacketSize();
    } else {
        // ERROR!
        // 1 = initial memory load failed
        // 2 = DMP configuration updates failed
        // (if it's going to break, usually the code will be 1)
        // Serial.print(F("DMP Initialization failed (code "));
        // Serial.print(devStatus);
        // Serial.println(F(")"));
        return ERROR_DMP_INIT_FAILED;
    }
#endif

#ifdef USE_PERIPHERAL_BMP280
    /* BMP 280 settings */
    if (!bmp.begin(BMP280_ADDRESS_ALT, BMP280_CHIPID))
        return ERROR_BMP_INIT_FAILED;

    bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,  /* Operating Mode. */
                    Adafruit_BMP280::SAMPLING_X2,  /* Temp. oversampling */
                    Adafruit_BMP280::SAMPLING_X16, /* Pressure oversampling */
                    Adafruit_BMP280::FILTER_X16,   /* Filtering. */
                    Adafruit_BMP280::STANDBY_MS_500); /* Standby time. (ms) */
#endif

#ifdef USE_MPU_ISP_INTERFACE
    // Chip select pin for MPU9250
    pinMode(PIN_SPI_CS_IMU, OUTPUT);
    digitalWrite(PIN_SPI_CS_IMU, HIGH);
#endif

#ifdef USE_PERIPHERAL_BMP280
    // Get sea level information
    double p = 0, t = 0;
    for (int i = 0; i < IMU_BMP_SEA_LEVEL_PRESSURE_SAMPLING; i++) {
        p += bmp.readPressure();
        t += bmp.readTemperature();
        delay(20);
    }
    // Assuming the initializing altitude is sea level
    // unit from pa to hpa
    seaLevelHpa = p / IMU_BMP_SEA_LEVEL_PRESSURE_SAMPLING / 100;
#endif
    return ERROR_OK;
}

#ifdef USE_PERIPHERAL_MPU6050

volatile bool mpuInterrupt =
    false;  // indicates whether MPU interrupt pin has gone high
void dmpDataReady()
{
    mpuInterrupt = true;
}

bool IMU::imu_isr_update()
{
    if (dmpReady) {
        // orientation/motion vars
        // [x, y, z]            accel sensor measurements
        VectorInt16 aa;
        // [x, y, z]            gravity-free accel sensor measurements
        VectorInt16 aaReal;

        // [x, y, z]            gravity vector
        VectorFloat gravity;
        // [psi, theta, phi]    Euler angle container
        float euler[3];
        // [yaw, pitch, roll]   yaw/pitch/roll container and gravity vector
        float ypr[3];

        if (mpu.dmpGetCurrentFIFOPacket(fifoBuffer)) {  // Get the Latest packet
            mpu.dmpGetQuaternion(&q, fifoBuffer);
            mpu.dmpGetAccel(&aa, fifoBuffer);
            mpu.dmpGetGravity(&gravity, &q);
            mpu.dmpGetLinearAccel(&aaReal, &aa, &gravity);
            mpu.dmpGetLinearAccelInWorld(&aaWorld, &aaReal, &q);

            /*Serial.print(aaWorld.x);
            Serial.print("\t");
            Serial.print(aaWorld.y);
            Serial.print("\t");
            Serial.println(aaWorld.z);*/

            float aaSize = aaReal.getMagnitude();
            /*if (aaSize > IMU_ACCEL_CRITERIA_MAGNITUDE) {
                // moving
                gravity.normalize();
                float inner = (gravity.x * aaReal.x + gravity.y * aaReal.y +
                               gravity.z * aaReal.z) /
                              aaSize;

                Serial.print(aaReal.x);
                Serial.print(",");
                Serial.print(aaReal.y);
                Serial.print(",");
                Serial.println(aaReal.z);
                Serial.println(inner);
                if (inner > IMU_ACCEL_CRITERIA_INNER)
                    pose = ROCKET_FALLING;
                // else if (inner < -IMU_ACCEL_CRITERIA_INNER)
                //    pose = ROCKET_RISING;
            } else
                pose = ROCKET_UNKNOWN;*/

            mpu_last_update_time = millis() & 0x0FFF;
            imu_update_flag = 0xFF;
            return true;
        }
    }
    return false;
}
#endif

float IMU::altitude_filter(float v)
{
    static float decay = v;
    decay = IMU_ALTITUDE_SMOOTHING_CONSTANT * decay +
            (1 - IMU_ALTITUDE_SMOOTHING_CONSTANT) * v;
    return decay;
}

#ifdef USE_PERIPHERAL_BMP280
/* The criteria to determine launching state:
 * 1. The average first derivative of altitude is larger than
 */
void IMU::bmp_update()
{
    altitude = altitude_filter(bmp.readAltitude(seaLevelHpa));
    // altitude = bmp.readAltitude(seaLevelHpa);

    static float lastAltitude = altitude;

    // times 1000 because unit changes from ms to s
    // TODO: use third derivative
    float derivative =
        1000 * (altitude - lastAltitude) / IMU_BMP_SAMPLING_PERIOD;

    // Updating altitude record
    lastAltitude = altitude;

    // Digital Filter for altitude
    static uint8_t rising_filter = 0;
    static uint8_t falling_filter = 0;

    rising_filter |= (derivative > IMU_RISING_CRITERIA);
    falling_filter |= (derivative < IMU_FALLING_CRITERIA);

    // Rocket rising
    if ((rising_filter & 0b0111) == 0b0111)
        pose = ROCKET_RISING;
    // Rocket falling
    else if ((falling_filter & 0b0111) == 0b0111)
        pose = ROCKET_FALLING;
    // Other
    else
        pose = ROCKET_UNKNOWN;

    rising_filter = rising_filter << 1;
    falling_filter = falling_filter << 1;
}
#endif