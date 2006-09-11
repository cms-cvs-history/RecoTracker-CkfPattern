#include <memory>
#include <string>

#include "FWCore/Framework/interface/Handle.h"
#include "FWCore/Framework/interface/ESHandle.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"

#include "DataFormats/Common/interface/OwnVector.h"
#include "DataFormats/TrajectorySeed/interface/TrajectorySeedCollection.h"
#include "DataFormats/TrackCandidate/interface/TrackCandidateCollection.h"

#include "Geometry/TrackerGeometryBuilder/interface/TrackerGeometry.h"

#include "TrackingTools/PatternTools/interface/Trajectory.h"
#include "TrackingTools/TrajectoryCleaning/interface/TrajectoryCleanerBySharedHits.h"
#include "TrackingTools/Records/interface/TrackingComponentsRecord.h"
#include "TrackingTools/TrajectoryState/interface/TrajectoryStateTransform.h"

#include "RecoTracker/CkfPattern/interface/CkfTrackCandidateMaker.h"
#include "RecoTracker/CkfPattern/interface/TransientInitialStateEstimator.h"
#include "RecoTracker/Record/interface/TrackerRecoGeometryRecord.h"



using namespace edm;
using namespace std;

namespace cms{
  CkfTrackCandidateMaker::CkfTrackCandidateMaker(edm::ParameterSet const& conf) : 
    conf_(conf),theCkfTrajectoryBuilder(0),theTrajectoryCleaner(0),
    theInitialState(0),theMeasurementTracker(0),theNavigationSchool(0)
  {  
    produces<TrackCandidateCollection>();  
  }

  
  // Virtual destructor needed.
  CkfTrackCandidateMaker::~CkfTrackCandidateMaker() {
    delete theInitialState;
    delete theMeasurementTracker;
    delete theNavigationSchool;
    delete theCkfTrajectoryBuilder;
    delete theTrajectoryCleaner;    
  }  

  void CkfTrackCandidateMaker::beginJob (EventSetup const & es)
  {
    //services
    es.get<TrackerRecoGeometryRecord>().get( theGeomSearchTracker );
    es.get<IdealMagneticFieldRecord>().get(theMagField);
      
    // get nested parameter set for the TransientInitialStateEstimator
    ParameterSet tise_params = conf_.getParameter<ParameterSet>("TransientInitialStateEstimatorParameters") ;
    theInitialState          = new TransientInitialStateEstimator( es,tise_params);
    
    // get nested parameter set for the MeasurementTracker
    ParameterSet mt_params = conf_.getParameter<ParameterSet>("MeasurementTrackerParameters") ;
    theMeasurementTracker = new MeasurementTracker(es, mt_params);

    theNavigationSchool   = new SimpleNavigationSchool(&(*theGeomSearchTracker),&(*theMagField));
      
    // set the correct navigation
    NavigationSetter setter( *theNavigationSchool);

    theCkfTrajectoryBuilder = new CkfTrajectoryBuilder(conf_,es,theMeasurementTracker);
    theTrajectoryCleaner = new TrajectoryCleanerBySharedHits();    
  }
  
  // Functions that gets called by framework every event
  void CkfTrackCandidateMaker::produce(edm::Event& e, const edm::EventSetup& es)
  {        
    // Step A: update MeasurementTracker
    theMeasurementTracker->update(e);
        
    
    // Step B: Retrieve seeds
    
    std::string seedProducer = conf_.getParameter<std::string>("SeedProducer");
    edm::Handle<TrajectorySeedCollection> collseed;
    e.getByLabel(seedProducer, collseed);
    //    e.getByType(collseed);
    TrajectorySeedCollection theSeedColl = *collseed;
    
    // Step C: Create empty output collection
    std::auto_ptr<TrackCandidateCollection> output(new TrackCandidateCollection);    
    
    
    // Step D: Invoke the building algorithm
    if ((*collseed).size()>0){
      vector<Trajectory> theFinalTrajectories;
      TrajectorySeedCollection::const_iterator iseed;
      
      vector<Trajectory> rawResult;
      for(iseed=theSeedColl.begin();iseed!=theSeedColl.end();iseed++){
	vector<Trajectory> theTmpTrajectories;
	theTmpTrajectories = theCkfTrajectoryBuilder->trajectories(*iseed);
	
       
	LogDebug("CkfPattern") << "CkfTrajectoryBuilder returned " << theTmpTrajectories.size()
			       << " trajectories for this seed";

	theTrajectoryCleaner->clean(theTmpTrajectories);
      
	for(vector<Trajectory>::const_iterator it=theTmpTrajectories.begin();
	    it!=theTmpTrajectories.end(); it++){
	  if( it->isValid() ) {
	    rawResult.push_back(*it);
	  }
	}
	LogDebug("CkfPattern") << "rawResult size after cleaning " << rawResult.size();
      }
      
      // Step E: Clean the result
      vector<Trajectory> unsmoothedResult;
      theTrajectoryCleaner->clean(rawResult);
      
      for (vector<Trajectory>::const_iterator itraw = rawResult.begin();
	   itraw != rawResult.end(); itraw++) {
	if((*itraw).isValid()) unsmoothedResult.push_back( *itraw);
      }
      //analyseCleanedTrajectories(unsmoothedResult);
      

      // Step F: Convert to TrackCandidates
      for (vector<Trajectory>::const_iterator it = unsmoothedResult.begin();
	   it != unsmoothedResult.end(); it++) {
	
	OwnVector<TrackingRecHit> recHits;
	Trajectory::RecHitContainer thits = it->recHits();
	for (Trajectory::RecHitContainer::const_iterator hitIt = thits.begin();
	     hitIt != thits.end(); hitIt++) {
	  recHits.push_back( (**hitIt).hit()->clone());
	}
	
	//PTrajectoryStateOnDet state = *(it->seed().startingState().clone());
	std::pair<TrajectoryStateOnSurface, const GeomDet*> initState = 
	  theInitialState->innerState( *it);
      
	// temporary protection againt invalid initial states
	if (! initState.first.isValid() || initState.second == 0) {
          //cout << "invalid innerState, will not make TrackCandidate" << endl;
          continue;
        }

	PTrajectoryStateOnDet* state = TrajectoryStateTransform().persistentState( initState.first,
										   initState.second->geographicalId().rawId());
	//	FitTester fitTester(es);
	//	fitTester.fit( *it);
	
	output->push_back(TrackCandidate(recHits,it->seed(),*state));
	delete state;
      }
      
      
      
      edm::LogVerbatim("CkfPattern") << "========== CkfTrackCandidateMaker Info ==========";
      edm::ESHandle<TrackerGeometry> tracker;
      es.get<TrackerDigiGeometryRecord>().get(tracker);
      edm::LogVerbatim("CkfPattern") << "number of Seed: " << theSeedColl.size();
      
      /*
      for(iseed=theSeedColl.begin();iseed!=theSeedColl.end();iseed++){
	DetId tmpId = DetId( iseed->startingState().detId());
	const GeomDet* tmpDet  = tracker->idToDet( tmpId );
	GlobalVector gv = tmpDet->surface().toGlobal( iseed->startingState().parameters().momentum() );
	
	edm::LogVerbatim("CkfPattern") << "seed perp,phi,eta : " 
				       << gv.perp() << " , " 
				       << gv.phi() << " , " 
				       << gv.eta() ;
      }
      */
      
      edm::LogVerbatim("CkfPattern") << "number of finalTrajectories: " << unsmoothedResult.size();
      for (vector<Trajectory>::const_iterator it = unsmoothedResult.begin();
	   it != unsmoothedResult.end(); it++) {
	edm::LogVerbatim("CkfPattern") << "n valid and invalid hit, chi2 : " 
	     << it->foundHits() << " , " << it->lostHits() <<" , " <<it->chiSquared();
      }
      edm::LogVerbatim("CkfPattern") << "=================================================";
          
    }
    // Step G: write output to file
    e.put(output);
  }
}

