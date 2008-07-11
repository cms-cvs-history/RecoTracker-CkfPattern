import FWCore.ParameterSet.Config as cms

# initialize magnetic field #########################
#include "Geometry/CMSCommonData/data/cmsMagneticFieldXML.cfi"
from MagneticField.Engine.uniformMagneticField_cfi import *
# initialize geometry #####################
from RecoTracker.GeometryESProducer.TrackerRecoGeometryESProducer_cfi import *
# KFUpdatoerESProducer
from TrackingTools.KalmanUpdators.KFUpdatorESProducer_cfi import *
# Chi2MeasurementEstimatorESProducer
from TrackingTools.KalmanUpdators.Chi2MeasurementEstimatorESProducer_cfi import *
# PropagatorWithMaterialESProducer
from TrackingTools.MaterialEffects.MaterialPropagator_cfi import *
# PropagatorWithMaterialESProducer
from TrackingTools.MaterialEffects.OppositeMaterialPropagator_cfi import *
# stripCPE
from RecoLocalTracker.SiStripRecHitConverter.StripCPE_cfi import *
from RecoLocalTracker.SiStripRecHitConverter.StripCPEfromTrackAngle_cfi import *
from RecoLocalTracker.SiStripRecHitConverter.SiStripRecHitMatcher_cfi import *
# pixelCPE
from RecoLocalTracker.SiPixelRecHits.PixelCPEParmError_cfi import *
#TransientTrackingBuilder
from RecoTracker.TransientTrackingRecHit.TransientTrackingRecHitBuilder_cfi import *
import copy
from RecoTracker.MeasurementDet.MeasurementTrackerESProducer_cfi import *
# MeasurementTracker
CTF_TIF_MeasurementTracker = copy.deepcopy(MeasurementTracker)
# trajectory filtering
from TrackingTools.TrajectoryFiltering.TrajectoryFilterESProducer_cff import *
import copy
from TrackingTools.TrajectoryFiltering.TrajectoryFilterESProducer_cfi import *
ckfBaseTrajectoryFilterTIF = copy.deepcopy(trajectoryFilterESProducer)
import copy
from RecoTracker.CkfPattern.GroupedCkfTrajectoryBuilderESProducer_cfi import *
#
GroupedCkfTrajectoryBuilderTIF = copy.deepcopy(GroupedCkfTrajectoryBuilder)
CTF_TIF_MeasurementTracker.ComponentName = 'CTF_TIF'
CTF_TIF_MeasurementTracker.pixelClusterProducer = ''
ckfBaseTrajectoryFilterTIF.ComponentName = 'ckfBaseTrajectoryFilterTIF'
ckfBaseTrajectoryFilterTIF.filterPset.minPt = 0.01
#replace ckfBaseTrajectoryFilterTIF.filterPset.maxLostHits = 3
#replace ckfBaseTrajectoryFilterTIF.filterPset.maxConsecLostHits = 1
ckfBaseTrajectoryFilterTIF.filterPset.minimumNumberOfHits = 4
GroupedCkfTrajectoryBuilderTIF.MeasurementTrackerName = 'CTF_TIF'
GroupedCkfTrajectoryBuilderTIF.ComponentName = 'GroupedCkfTrajectoryBuilderTIF'
GroupedCkfTrajectoryBuilderTIF.trajectoryFilterName = 'ckfBaseTrajectoryFilterTIF'

