#include "Rotor.h"

Rotor::Rotor(double home, int homeAddr, double min, int minAddr, double max, int maxAddr, int gpioPinRight, int gpioPinLeft, int gpioPinPoti, int potiTolerance, int numReadings)
    : current(0), target(0), home(0), homeAddr(homeAddr), min(0), minAddr(minAddr), max(0), maxAddr(maxAddr),
      gpioPinRight(gpioPinRight), gpioPinLeft(gpioPinLeft), gpioPinPoti(gpioPinPoti), potiTolerance(potiTolerance), numReadings(numReadings),
      readIndex(0), total(0), average(0)
{
    readings = new int[numReadings];

    for (int i = 0; i < numReadings; i++)
    {
        readings[i] = 0;
    }
}

Rotor::~Rotor()
{
    delete[] readings;
}

void Rotor::initialize()
{
    pinMode(gpioPinRight, OUTPUT);
    pinMode(gpioPinLeft, OUTPUT);
    pinMode(gpioPinPoti, INPUT);

    stop();

    this->home = home;
    this->min = min;
    this->max = max;

    total = 0;

    for (int i = 0; i < numReadings; i++)
    {
        readings[i] = 0;
    }
}

double Rotor::getCurrent() const
{
    return current;
}

double Rotor::getTarget() const
{
    return target;
}

void Rotor::setTarget(double value)
{
    this->target = value;
}

double Rotor::getHome() const
{
    return home;
}

int Rotor::getHomeAddr()
{
    return homeAddr;
}

void Rotor::setHome(double value)
{
    this->home = value;
}

void Rotor::updatePosition()
{
    total = total - readings[readIndex];
    readings[readIndex] = analogRead(gpioPinPoti) / 4;
    total = total + readings[readIndex];

    readIndex++;
    if (readIndex >= numReadings)
    {
        readIndex = 0;
    }

    average = total / numReadings;
    current = average;

    if (abs(target - current) <= potiTolerance)
    {
        stop();
    }
    else if (target > current)
    {
        moveRight();
    }
    else
    {
        moveLeft();
    }
}

void Rotor::moveLeft()
{
    digitalWrite(gpioPinRight, LOW);
    digitalWrite(gpioPinLeft, HIGH);
}

void Rotor::moveRight()
{
    digitalWrite(gpioPinRight, HIGH);
    digitalWrite(gpioPinLeft, LOW);
}

void Rotor::moveHome()
{
    this->setTarget(this->getHome());
}

void Rotor::stop()
{
    digitalWrite(gpioPinRight, LOW);
    digitalWrite(gpioPinLeft, LOW);
}

void Rotor::findMin()
{
    int minValue = analogRead(gpioPinPoti);
    int stableCount = 0;

    moveLeft();

    while (stableCount < 3)
    {
        int currentValue = analogRead(gpioPinPoti);
        if (currentValue < minValue - potiTolerance)
        {
            minValue = currentValue;
            stableCount = 0;
        }
        else
        {
            stableCount++;
        }
        if (stableCount < 3)
        {
            delay(1000);
        }
    }

    stop();

    min = minValue;
    EEPROM.write(minAddr, minValue);
    EEPROM.commit();
}

void Rotor::setMin(double value)
{
    this->min = value;
}

double Rotor::getMin()
{
    return min;
}

int Rotor::getMinAddr()
{
    return minAddr;
}

void Rotor::findMax()
{
    int maxValue = analogRead(gpioPinPoti);
    int stableCount = 0;

    moveRight();

    while (stableCount < 3)
    {
        int currentValue = analogRead(gpioPinPoti);
        if (currentValue > maxValue + potiTolerance)
        {
            maxValue = currentValue;
            stableCount = 0;
        }
        else
        {
            stableCount++;
        }
        if (stableCount < 3)
        {
            delay(1000);
        }
    }

    stop();

    max = maxValue;
    EEPROM.write(maxAddr, maxValue);
    EEPROM.commit();
}

void Rotor::setMax(double value)
{
    this->max = value;
}

double Rotor::getMax()
{
    return max;
}

int Rotor::getMaxAddr()
{
    return maxAddr;
}

void Rotor::reset()
{
    target = home;
}
