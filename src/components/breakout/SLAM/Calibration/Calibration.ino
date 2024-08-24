#include "Wire.h"
#include <QMC5883LCompass.h>

//#define DEBUG1
//#define DEBUG2


class calibration{
  private:
    double lr = 0.00001;                 // learning rate
    double b[4] = {0.0, 0.0, 0.0, 1.0};  // bias: [x, y, z, r]
    double f = 0.0;                      // residual = (xi-x)^2 + (yi-y)^2 + (zi-z)^2 - r^2
    double dx = 0.0;                     // 
    double dy = 0.0;                     //
    double dz = 0.0;                     //
  
  public:
    void update(double x, double y, double z);
    const double *get_bias();
};

void calibration::update(double x, double y, double z)
{
  dx = x - b[0];
  dy = y - b[1];
  dz = z - b[2];
  f = dx*dx + dy*dy + dz*dz - b[3]*b[3];
  b[0] = b[0] + 4*lr*f*dx;
  b[1] = b[1] + 4*lr*f*dy;
  b[2] = b[2] + 4*lr*f*dz;
  b[3] = b[3] + 4*lr*f*b[3];
}

const double *calibration::get_bias()
{
  return b;
}

QMC5883LCompass compass;
calibration Cal;

void setup()
{
  Serial.begin(9600);
  delay(1000);
  Wire.setSDA(4);
  Wire.setSCL(5);
  compass.init();
}

void loop()
{
  compass.read();

  int x = compass.getX();
  int y = compass.getY();
  int z = compass.getZ();
  double X = (double)x/1000.0;
  double Y = (double)y/1000.0;
  double Z = (double)z/1000.0;


  #ifdef DEBUG1
    Serial.print(X, 8);
    Serial.print("\t");
    Serial.print(Y, 8);
    Serial.print("\t");
    Serial.print(Z, 8);
    Serial.println();
  #endif

  Cal.update(X, Y, Z);
  const double *b = Cal.get_bias();

  #ifdef DEBUG2
  for(int i = 0; i < 4; i++){
    Serial.print(b[i], 8);
    Serial.print("\t");
  }
  Serial.println();
  #endif

  x1 = 1000.0*(X-b[0])*b[3];
  y1 = 1000.0*(Y-b[1])*b[3];
  z1 = 1000.0*(Z-b[2])*b[3];
 
}