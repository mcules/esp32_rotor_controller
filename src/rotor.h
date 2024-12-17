#ifndef ROTOR_H
#define ROTOR_H

#include <EEPROM.h>
#include <Arduino.h>

class Rotor
{
private:
    double current;
    double target;
    double home;
    int homeAddr;
    double min;
    int minAddr;
    double max;
    int maxAddr;
    int gpioPinRight;
    int gpioPinLeft;
    int gpioPinPoti;

    int *readings;
    int numReadings;
    int readIndex;
    int total;
    int average;
    int potiTolerance;

public:
    Rotor(double home, int homeAddr, double min, int minAddr, double max, int maxAddr, int gpioPinRight, int gpioPinLeft, int gpioPinPoti, int potiTolerance, int numReadings);

    ~Rotor();

    void initialize();
    double getCurrent() const;
    double getTarget() const;
    double getHome() const;
    int getHomeAddr();
    void setTarget(double value);
    void setHome(double value);
    void updatePosition();
    void findMin();
    void setMin(double value);
    double getMin();
    int getMinAddr();
    void setMax(double value);
    double getMax();
    int getMaxAddr();
    void findMax();
    void reset();
    void moveLeft();
    void moveRight();
    void moveHome();
    void stop();
};

#endif
