//
//    FILE: CHT8310.cpp
//  AUTHOR: Rob Tillaart
// VERSION: 0.2.0
// PURPOSE: Arduino library for CHT8310 temperature and humidity sensor
//     URL: https://github.com/RobTillaart/CHT8310


#include "CHT8310.h"
#include <math.h>


//  REGISTERS
#define CHT8310_REG_TEMPERATURE          0x00
#define CHT8310_REG_HUMIDITY             0x01
#define CHT8310_REG_STATUS               0x02
#define CHT8310_REG_CONFIG               0x03
#define CHT8310_REG_CONVERT_RATE         0x04
#define CHT8310_REG_TEMP_HIGH_LIMIT      0x05
#define CHT8310_REG_TEMP_LOW_LIMIT       0x06
#define CHT8310_REG_HUM_HIGH_LIMIT       0x07
#define CHT8310_REG_HUM_LOW_LIMIT        0x08
#define CHT8310_REG_ONESHOT              0x0F

#define CHT8310_REG_SWRESET              0xFC
#define CHT8310_REG_MANUFACTURER         0xFF



/////////////////////////////////////////////////////
//
// PUBLIC
//
CHT8310::CHT8310(const uint8_t address)
{
  _address = address;
}


int CHT8310::begin(i2c_port_t port, int sda, int scl)
{
  //  address = 0x40, 0x44, 0x48, 0x4C
  if ((_address != 0x40) && (_address != 0x44) && (_address != 0x48) && (_address != 0x4C))
  {
    return CHT8310_ERROR_ADDR;
  }
    i2c_config_t conf{};

    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = sda;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = scl;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = 400000;

    bus = i2c_bus_create(port, &conf);
    assert(bus != nullptr);
    device = i2c_bus_device_create(bus, _address, 0);
    if (device == nullptr)
        return CHT8310_ERROR_CONNECT;
    return CHT8310_OK;
}

uint8_t CHT8310::getAddress()
{
  return _address;
}


int CHT8310::readTemperature()
{
  int16_t tmp = readRegister(CHT8310_REG_TEMPERATURE);
  //  DATASHEET P13
  if (_resolution == 13)
  {
    _temperature = (tmp >> 3) * 0.03125;
  }
  else  //  _resolution == 14
  {
    _temperature = (tmp >> 2) * 0.03125;
  }

  if (_tempOffset != 0.0)
  {
    _temperature += _tempOffset;
  }

  return CHT8310_OK;
}


int CHT8310::readHumidity()
{
  int16_t tmp = readRegister(CHT8310_REG_HUMIDITY);

  //  DATASHEET P14
  if (tmp & 0x8000)  //  test overflow bit
  {
    _humidity = 100.0;
    return CHT8310_ERROR_HUMIDITY;
  }
  tmp &= 0x7FFF;
  _humidity = tmp * (1.0 / 327.67);  //  == / 32767 * 100%
  //  Handle humidity offset.
  if (_humOffset  != 0.0)
  {
    _humidity += _humOffset;
    //  handle out of range.
    if (_humidity < 0.0)   _humidity = 0.0;
    if (_humidity > 100.0) _humidity = 100.0;
  }

  return CHT8310_OK;
}


//  MilliSeconds since start sketch
uint32_t CHT8310::lastRead()
{
  return _lastRead;
}


float CHT8310::getHumidity()
{
  return _humidity;
}


float CHT8310::getTemperature()
{
  return _temperature;
}


void CHT8310::setConversionDelay(uint8_t cd)
{
  if (cd < 8) cd = 8;
  _conversionDelay = cd;
}


uint8_t CHT8310::getConversionDelay()
{
  return _conversionDelay;
}


////////////////////////////////////////////////
//
//  OFFSET
//
void CHT8310::setHumidityOffset(float offset)
{
  _humOffset = offset;
}


void CHT8310::setTemperatureOffset(float offset)
{
  _tempOffset = offset;
}


float CHT8310::getHumidityOffset()
{
  return _humOffset;
}


float CHT8310::getTemperatureOffset()
{
  return _tempOffset;
}


////////////////////////////////////////////////
//
//  CONFIGURATION
//
void CHT8310::setConfiguration(uint16_t mask)
{
  writeRegister(CHT8310_REG_CONFIG, mask);
}


uint16_t CHT8310::getConfiguration()
{
  return readRegister(CHT8310_REG_CONFIG);
}


////////////////////////////////////////////////
//
//  CONVERT RATE
//
void CHT8310::setConvertRate(uint8_t rate)
{
  if (rate > 7) rate = 7;
  writeRegister(CHT8310_REG_CONVERT_RATE, ((uint16_t)rate) << 8);
}


uint8_t CHT8310::getConvertRate()
{
  return (readRegister(CHT8310_REG_CONVERT_RATE) >> 8) & 0x07;
}


////////////////////////////////////////////////
//
//  ALERT
//
void CHT8310::setTemperatureHighLimit(float temperature)
{
  int16_t tmp = round(temperature * (1.0 / 0.03125));
  tmp <<= 3;
  writeRegister(CHT8310_REG_TEMP_HIGH_LIMIT, tmp);
}

void CHT8310::setTemperatureLowLimit(float temperature)
{
  int16_t tmp = round(temperature * (1.0 / 0.03125));
  tmp <<= 3;
  writeRegister(CHT8310_REG_TEMP_LOW_LIMIT, tmp);
}

void CHT8310::setHumidityHighLimit(float humidity)
{
  int16_t hum = round(humidity * 327.67);
  writeRegister(CHT8310_REG_HUM_HIGH_LIMIT, hum);
}

void CHT8310::setHumidityLowLimit(float humidity)
{
  int16_t hum = round(humidity * 327.67);
  writeRegister(CHT8310_REG_HUM_LOW_LIMIT, hum);
}


////////////////////////////////////////////////
//
//  STATUS
//
uint16_t CHT8310::getStatusRegister()
{
  return readRegister(CHT8310_REG_STATUS);
}


////////////////////////////////////////////////
//
//  ONE SHOT
//
void CHT8310::oneShotConversion()
{
  writeRegister(CHT8310_REG_ONESHOT, 0xFFFF);
}


////////////////////////////////////////////////
//
//  SOFTWARE RESET
//
void CHT8310::softwareReset()
{
  writeRegister(CHT8310_REG_SWRESET, 0xFFFF);
}


////////////////////////////////////////////////
//
//  META DATA
//
uint16_t CHT8310::getManufacturer()
{
  return readRegister(CHT8310_REG_MANUFACTURER);
}


////////////////////////////////////////////////
//
//  ACCESS REGISTERS
//
uint16_t CHT8310::readRegister(uint8_t reg)
{
  uint8_t data[2] = { 0, 0 };
  _readRegister(reg, &data[0], 2);
  uint16_t tmp = data[0] << 8 | data[1];
  return tmp;
}


int CHT8310::writeRegister(uint8_t reg, uint16_t value)
{
  uint8_t data[2];
  data[1] = value & 0xFF;
  data[0] = value >> 8;
  return _writeRegister(reg, data, 2);
}


////////////////////////////////////////////////
//
//  PRIVATE
//
int CHT8310::_readRegister(uint8_t reg, uint8_t * buf, uint8_t size)
{
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_bus_read_bytes(device, reg, size, buf));
    return CHT8310_OK;
}


int CHT8310::_writeRegister(uint8_t reg, uint8_t * buf, uint8_t size)
{
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_bus_write_bytes(device, reg, size, buf));
    return CHT8310_OK;
}


//  -- END OF FILE --

