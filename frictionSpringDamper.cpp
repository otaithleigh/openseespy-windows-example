/* ****************************************************************** **
**    OpenSees - Open System for Earthquake Engineering Simulation    **
**          Pacific Earthquake Engineering Research Center            **
**                                                                    **
**                                                                    **
** (C) Copyright 1999, The Regents of the University of California    **
** All Rights Reserved.                                               **
**                                                                    **
** Commercial use of this program without express permission of the   **
** University of California, Berkeley, is strictly prohibited.  See   **
** file 'COPYRIGHT'  in main directory for information on usage and   **
** redistribution,  and for a DISCLAIMER OF ALL WARRANTIES.           **
**                                                                    **
** Developed by:                                                      **
**   Frank McKenna (fmckenna@ce.berkeley.edu)                         **
**   Gregory L. Fenves (fenves@ce.berkeley.edu)                       **
**   Filip C. Filippou (filippou@ce.berkeley.edu)                     **
**                                                                    **
** ****************************************************************** */

// Documentation: Mroz Multi-Surface Kinematic Hardening Material
//
// uniaxialMaterial frictionSpringDamper $tag $Ke $K $Kp $preload <-initialStrain $initStrain>
//
// Required Input Parameters: 
//   $tag         integer tag identifying material
//   $Ke          elastic stiffness
//   $K           loading stiffness
//   $Kp          unloading stiffness
//   $preload     preload
//   $initStrain  initial strain (optional, default = 0.0)
//
// References:
//   1. Wang, W., Fang, C., Zhao, Y., Sause, R., Hu, S., and Ricles, J. (2019). 
//      “Self-centering friction spring dampers for seismic resilience.” 
//      Earthquake Engineering & Structural Dynamics, 48(9), 1045–1065.

#include <elementAPI.h>
#include "frictionSpringDamper.h"

#include <Vector.h>
#include <Channel.h>
#include <math.h>
#include <float.h>


#ifdef _USRDLL
#define OPS_Export extern "C" _declspec(dllexport)
#elif _MACOSX
#define OPS_Export extern "C" __attribute__((visibility("default")))
#else
#define OPS_Export extern "C"
#endif

OPS_Export void
localInit()
{
  OPS_Error("frictionSpringDamper unaxial material \nWritten by Mark D. Denavit, University of Tennessee, Knoxville\n", 1);
}

OPS_Export void *
OPS_frictionSpringDamper()
{
  // Pointer to a uniaxial material that will be returned
  UniaxialMaterial *theMaterial = 0;

  int numData;

  numData = 1;
  int tag;
  if (OPS_GetIntInput(&numData, &tag) != 0) {
    opserr << "WARNING invalid uniaxialMaterial frictionSpringDamper tag \n";
    return 0;
  }
    
  numData = 4;
  double dData[4];
  if (OPS_GetDoubleInput(&numData, dData) != 0) {
    opserr << "WARNING invalid data for uniaxialMaterial frictionSpringDamper with tag " << tag << endln;
    return 0;
  }    
  double Ke = dData[0];
  double K = dData[1];
  double Kp = dData[2];
  double preload = dData[3];
  double initStrain = 0.0;

  // Loop through remaining arguments
  int sDataLength = 40;
  char *sData = new char[sDataLength];
  while ( OPS_GetNumRemainingInputArgs() > 0 ) {
    if ( OPS_GetStringCopy(&sData) != 0 ) {
      opserr << "WARNING invalid input";
      return 0;
    }

    if ( strcmp(sData,"-initialStrain") == 0 ) {
      numData = 1;
      if (OPS_GetDoubleInput(&numData, dData) != 0) {
        opserr << "WARNING invalid input, want: -initialStrain $initStrain \n";
        return 0;
      }
      initStrain = dData[0];
    } else {
      opserr << "WARNING unknown option " << sData << "\n";
      return 0;
    }
  }

  // Create material
  theMaterial = new frictionSpringDamper(tag, Ke, K, Kp, preload, initStrain);

  if (theMaterial == 0) {
    opserr << "WARNING could not create uniaxialMaterial of type frictionSpringDamper\n";
    return 0;
  }

  delete[] sData;

  return theMaterial;
}




frictionSpringDamper::frictionSpringDamper(int tag, double iKe, double iK, double iKp, double ipreload, double iinitStrain)
:UniaxialMaterial(tag,MAT_TAG_frictionSpringDamper),
 Ke(iKe), K(iK), Kp(iKp), preload(ipreload), initStrain(iinitStrain)
{
  // Set Remaining Variables
  this->revertToStart();
}

frictionSpringDamper::frictionSpringDamper()
:UniaxialMaterial(0,MAT_TAG_frictionSpringDamper),
 Ke(0.0), K(0.0), Kp(0.0), preload(0.0), initStrain(0.0)
{
  // Set Remaining Variables
  this->revertToStart();
}

frictionSpringDamper::~frictionSpringDamper()
{
  // does nothing
}

int frictionSpringDamper::setTrialStrain(double strain, double strainRate) {

  trialStrain = strain + initStrain;

  double strainIncr = trialStrain - commitStrain;
  double F1 = preload;
  double F2 = preload*Kp/K;
  double Keq1 = 1.0/(1.0/Ke+1.0/K);
  double Keq2 = 1.0/(1.0/Ke+1.0/Kp);
  double backStrain = commitStrain - commitStress/Ke;

  if (strainIncr >= 0.0) {
    if (backStrain < 0.0) {
      if ( trialStrain < (backStrain + (backStrain*Kp-F2)/Ke) ) {
        trialStress = (trialStrain-backStrain)*Ke;
        trialTangent = Ke;
      } else if ( trialStrain < -F2/Ke ) {
        trialStress = -F2 + (trialStrain+F2/Ke)*Keq2;
        trialTangent = Keq2;
      } else if ( trialStrain < F1/Ke ) {
        trialStress = trialStrain*Ke;
        trialTangent = Ke;
      } else {
        trialStress = F1 + (trialStrain-F1/Ke)*Keq1;
        trialTangent = Keq1;
      }
    } else {
      if ( trialStrain < (backStrain + (F1+backStrain*K)/Ke) ) {
        trialStress = (trialStrain-backStrain)*Ke;
        trialTangent = Ke;
      } else {
        trialStress = F1 + (trialStrain-F1/Ke)*Keq1;
        trialTangent = Keq1;
      }
    }

  } else {
    if (backStrain > 0.0) {
      if ( trialStrain > (backStrain + (backStrain*Kp+F2)/Ke) ) {
        trialStress = (trialStrain-backStrain)*Ke;
        trialTangent = Ke;
      } else if ( trialStrain > F2/Ke ) {
        trialStress = F2 + (trialStrain-F2/Ke)*Keq2;
        trialTangent = Keq2;
      } else if ( trialStrain > -F1/Ke ) {
        trialStress = trialStrain*Ke;
        trialTangent = Ke;
      } else {
        trialStress = -F1 + (trialStrain+F1/Ke)*Keq1;
        trialTangent = Keq1;
      }
    } else {
      if ( trialStrain > (backStrain + (-F1+backStrain*K)/Ke) ) {
        trialStress = (trialStrain-backStrain)*Ke;
        trialTangent = Ke;
      } else {
        trialStress = -F1 + (trialStrain+F1/Ke)*Keq1;
        trialTangent = Keq1;
      }
    }

  }

  return 0;
}

double frictionSpringDamper::getStrain(void) {
  return trialStrain;
}

double frictionSpringDamper::getStress(void) {
  return trialStress;
}

double frictionSpringDamper::getTangent(void) {
  return trialTangent;
}

double frictionSpringDamper::getInitialTangent(void) {
  double Kinit;
  if (initStrain < -preload/Ke) {
    Kinit = K;
  } else if (initStrain <= preload/Ke) {
    Kinit = Ke;
  } else {
    Kinit = K;
  } 
  return Kinit;
}

int frictionSpringDamper::commitState(void) {
  commitStrain = trialStrain;
  commitStress = trialStress;
  commitTangent = trialTangent;
  return 0;
}

int frictionSpringDamper::revertToLastCommit(void) {
  trialStrain =  commitStrain;
  trialStress = commitStress;
  trialTangent = commitTangent;
  return 0;
}


int frictionSpringDamper::revertToStart(void) {
  if (initStrain < -preload/Ke) {
    trialTangent = K;
    trialStress = (initStrain+preload/Ke)*K - preload;
  } else if (initStrain <= preload/Ke) {
    trialTangent = Ke;
    trialStress = initStrain*Ke;
  } else {
    trialTangent = K;
    trialStress = (initStrain-preload/Ke)*K + preload;
  }
  trialStrain = initStrain;
  this->commitState();
  return 0;
}

UniaxialMaterial * frictionSpringDamper::getCopy(void) {
  frictionSpringDamper *theCopy = new frictionSpringDamper(this->getTag(), Ke, K, Kp, preload, initStrain);
  theCopy->trialStrain = this->trialStrain;
  theCopy->trialStress = this->trialStress;
  theCopy->trialTangent = this->trialTangent;
  theCopy->commitStrain = this->commitStrain;
  theCopy->commitStress = this->commitStress;
  theCopy->commitTangent = this->commitTangent;
  return theCopy;
}


int frictionSpringDamper::sendSelf(int cTag, Channel &theChannel) {
  opserr << "frictionSpringDamper::sendSelf() - not yet implemented\n";
  return -1;
}

int frictionSpringDamper::recvSelf(int cTag, Channel &theChannel, FEM_ObjectBroker &theBroker) {
  opserr << "frictionSpringDamper::recvSelf() - not yet implemented\n";
  return -1;
}

void frictionSpringDamper::Print(OPS_Stream &s, int flag) {
  s << "frictionSpringDamper tag: " << this->getTag() << endln;
  s << "  Ke: " << Ke << endln;
  s << "  K: " << K << endln;
  s << "  Kp: " << Kp << endln;
  s << "  preload: " << preload << endln;
  s << "  initStrain: " << initStrain << endln;
  s << "  stress: " << trialStress << " tangent: " << trialTangent << endln;
  return;
}

