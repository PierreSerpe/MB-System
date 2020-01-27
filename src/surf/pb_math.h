// DATEINAME        : hydEdit_math.h
// ERSTELLUNGSDATUM : 20.09.93
// COPYRIGHT (C) 1993: ATLAS ELEKTRONIK GMBH, 28305 BREMEN
//
// See README file for copying and redistribution conditions.

#ifndef SURF_PB_MATH_H_
#define SURF_PB_MATH_H_

#ifndef PI
#define PI 3.14159265359
#endif

#define HALF_PI (double)(3.14159265359/2.0)

#define RAD_TO_DEG(a) ((a>=+0.0)?                                \
                                (double)((180.0*a)/PI)           \
                               :                                 \
                                (double)(360.0 - ((180.0*a)/PI)) )

#define RAD_TO_PLUMINUS_DEG(a) ((double)((180.0*a)/PI))
#define RAD_TO_PLUMINUS_MIN(a) ((double)((180.0*60.0*a)/PI))

#define DEG_TO_RAD(a) ((a<=180.0)?                               \
                                (double)((PI*a)/180.0)           \
                                :                                \
                                (double)((PI*(a - 360.0))/180.0) )

#define DEG_TO_TWO_PI(a)        ((double)((PI*a)/180.0) )
#define MIN_TO_RAD(a)           ((double)((PI*a)/(180.0*60.0)) )
#define SET_TO_PLUS_PI(a) ((a<0.0)?                              \
                                ((double)(a+(2.0*PI)))           \
                                :                                \
                                ((double)(a))                    )

typedef  int   Boolean;
#define  True  1
#define  False 0

typedef struct {
                 double    x;
                 double    y;
               } XY_Coords;

typedef struct {
                 double    angle;
                 double    cmean;
                 double    ckeel;
                 double    travelTime;
                 double    draught;
                 double    heaveTx;
                 double    heaveRx;
                 double    pitchTx;
                 double    transducerOffsetStar;
                 double    transducerOffsetAhead;
                 double    depth;
                 double    posStar;
                 double    posAhead;
               } FanParam;

/* Plattkarten-Projektion */

#define M_PER_RAD_LAT      ((double)(60.0*180.0*1852.0/PI))
#define M_PER_RAD_LON(LAT) (((double)(60.0*180.0*1852.0/PI))*cos(LAT))

#define M_TO_RAD_Y(Y)      ((double)(Y/M_PER_RAD_LAT))
#define M_TO_RAD_X(X,LAT)  ((double)(X/(M_PER_RAD_LON(LAT))))
#define RAD_TO_METER_Y(LAT)      ((double)(LAT*M_PER_RAD_LAT))
#define RAD_TO_METER_X(LON,LAT)  ((double)(LON*(M_PER_RAD_LON(LAT))))

double pbAtan2(double y,double x);

double setToPlusMinusPI(double angle);
void rotateCoordinates(double rotAngle,XY_Coords* origCoords,
                                     XY_Coords* targetCoords);
void xyToRhoPhi(double x0,double y0,double pointX,double pointY,
                                      double* rho,double* phi);
void lambdaPhiToRhoPhi(double x0,double y0,double pointX,double pointY,
                                      double* rho,double* phi);
Boolean signf(double value);
Boolean signsh(short value);

Boolean depthFromTT(FanParam* fanParam,Boolean isPitchcompensated);
Boolean TTfromDepth(FanParam* fanParam,Boolean isPitchcompensated);
Boolean draughtFromDepth(FanParam* fanParam);
Boolean heaveFromDepth(FanParam* fanParam);

double cMeanToTemperature(double salinity,double cMean);
double temperatureToCMeanDelGrosso(double salinity,double temperature);
double temperatureToCMeanMedwin(double salinity,double temperature);
double temperatureToCMean(double salinity,double temperature);

SurfTime surfTimeOfDayFromAbsTime (SurfTime absTime);
void timeFromRelTime (SurfTime relTime,char*buffer);
Boolean relTimeFromTime (char*buffer,SurfTime* relTime);

#endif  // SURF_PB_MATH_H_
