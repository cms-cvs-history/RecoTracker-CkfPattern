#include <memory>
#include <string>

#include "DataFormats/Common/interface/Handle.h"
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

#include "RecoTracker/CkfPattern/interface/CkfTrackCandidateMakerBase.h"
#include "RecoTracker/CkfPattern/interface/TransientInitialStateEstimator.h"
#include "RecoTracker/Record/interface/TrackerRecoGeometryRecord.h"
#include "RecoTracker/Record/interface/CkfComponentsRecord.h"


#include "RecoTracker/CkfPattern/interface/SeedCleanerByHitPosition.h"
#include "RecoTracker/CkfPattern/interface/CachingSeedCleanerByHitPosition.h"
#include "RecoTracker/CkfPattern/interface/SeedCleanerBySharedInput.h"
#include "RecoTracker/CkfPattern/interface/CachingSeedCleanerBySharedInput.h"

#include "RecoTracker/TkNavigation/interface/NavigationSchoolFactory.h"

using namespace edm;
using namespace std;

namespace cms{
  CkfTrackCandidateMakerBase::CkfTrackCandidateMakerBase(edm::ParameterSet const& conf) : 

    conf_(conf),theTrajectoryBuilder(0),theTrajectoryCleaner(0),
    theInitialState(0),theNavigationSchool(0),theSeedCleaner(0)
  {  
    //produces<TrackCandidateCollection>();  
  }

  
  // Virtual destructor needed.
  CkfTrackCandidateMakerBase::~CkfTrackCandidateMakerBase() {
    delete theInitialState;  
    delete theNavigationSchool;
    delete theTrajectoryCleaner;    
    if (theSeedCleaner) delete theSeedCleaner;
  }  

  void CkfTrackCandidateMakerBase::beginJobBase (EventSetup const & es)
  {
    //services
    es.get<TrackerRecoGeometryRecord>().get( theGeomSearchTracker );
    es.get<IdealMagneticFieldRecord>().get(theMagField);
      
    // get nested parameter set for the TransientInitialStateEstimator
    ParameterSet tise_params = conf_.getParameter<ParameterSet>("TransientInitialStateEstimatorParameters") ;
    theInitialState          = new TransientInitialStateEstimator( es,tise_params);
    theTrajectoryCleaner     = new TrajectoryCleanerBySharedHits();          

    //theNavigationSchool      = new SimpleNavigationSchool(&(*theGeomSearchTracker),&(*theMagField));
    edm::ParameterSet NavigationPSet = conf_.getParameter<edm::ParameterSet>("NavigationPSet");
    std::string navigationSchoolName = NavigationPSet.getParameter<std::string>("ComponentName");
    theNavigationSchool = NavigationSchoolFactory::get()->create( navigationSchoolName, &(*theGeomSearchTracker), &(*theMagField));


    // set the correct navigation
    NavigationSetter setter( *theNavigationSchool);

    // set the TrajectoryBuilder
    std::string trajectoryBuilderName = conf_.getParameter<std::string>("TrajectoryBuilder");
    edm::ESHandle<TrackerTrajectoryBuilder> theTrajectoryBuilderHandle;
    es.get<CkfComponentsRecord>().get(trajectoryBuilderName,theTrajectoryBuilderHandle);
    theTrajectoryBuilder = theTrajectoryBuilderHandle.product();    
    
    std::string cleaner = conf_.getParameter<std::string>("RedundantSeedCleaner");
    if (cleaner == "SeedCleanerByHitPosition") {
        theSeedCleaner = new SeedCleanerByHitPosition();
    } else if (cleaner == "SeedCleanerBySharedInput") {
        theSeedCleaner = new SeedCleanerBySharedInput();
    } else if (cleaner == "CachingSeedCleanerByHitPosition") {
        theSeedCleaner = new CachingSeedCleanerByHitPosition();
    } else if (cleaner == "CachingSeedCleanerBySharedInput") {
        theSeedCleaner = new CachingSeedCleanerBySharedInput();
    } else if (cleaner == "none") {
        theSeedCleaner = 0;
    } else {
        throw cms::Exception("RedundantSeedCleaner not found", cleaner);
    }
  }
  
  // Functions that gets called by framework every event
  void CkfTrackCandidateMakerBase::produceBase(edm::Event& e, const edm::EventSetup& es)
  { 
    // method for Debugging
    printHitsDebugger(e);

    // Step A: set Event for the TrajectoryBuilder
    theTrajectoryBuilder->setEvent(e);        
    
    // Step B: Retrieve seeds
    
    std::string seedProducer = conf_.getParameter<std::string>("SeedProducer");
    std::string seedLabel = conf_.getParameter<std::string>("SeedLabel");
    edm::Handle<TrajectorySeedCollection> collseed;
    e.getByLabel(seedProducer, seedLabel, collseed);
    TrajectorySeedCollection theSeedColl = *collseed;
    
    // Step C: Create empty output collection
    std::auto_ptr<TrackCandidateCollection> output(new TrackCandidateCollection);    
    
    
    // Step D: Invoke the building algorithm
    if ((*collseed).size()>0){
      vector<Trajectory> theFinalTrajectories;
      TrajectorySeedCollection::const_iterator iseed;
      TrajectorySeedCollection::const_iterator end = lastSeed(theSeedColl);

      vector<Trajectory> rawResult;
      if (theSeedCleaner) theSeedCleaner->init( &rawResult );
      
      // method for debugging
      countSeedsDebugger();

      for(iseed=theSeedColl.begin();iseed!=end;iseed++){
	vector<Trajectory> theTmpTrajectories;
    
         if (theSeedCleaner && !theSeedCleaner->good(&(*iseed))) continue;
	theTmpTrajectories = theTrajectoryBuilder->trajectories(*iseed);
	
       
	LogDebug("CkfPattern") << "======== CkfTrajectoryBuilder returned " << theTmpTrajectories.size()
			       << " trajectories for this seed ========";

	theTrajectoryCleaner->clean(theTmpTrajectories);
      
	for(vector<Trajectory>::const_iterator it=theTmpTrajectories.begin();
	    it!=theTmpTrajectories.end(); it++){
	  if( it->isValid() ) {
	    rawResult.push_back(*it);
            if (theSeedCleaner) theSeedCleaner->add( & (*it) );
	  }
      
	}
	LogDebug("CkfPattern") << "rawResult size after cleaning " << rawResult.size();
      }
      
      if (theSeedCleaner) theSeedCleaner->done();
      
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
    
    // method for debugging
    deleteAssocDebugger();

    // Step G: write output to file
    e.put(output);
  }
}
