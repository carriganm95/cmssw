/* 
 *  \class EcalABAnalyzer
 *
 *  primary author: Julie Malcles - CEA/Saclay
 *  author: Gautier Hamel De Monchenault - CEA/Saclay
 */

#include <TAxis.h>
#include <TH1.h>
#include <TProfile.h>
#include <TTree.h>
#include <TChain.h>
#include <TFile.h>
#include <TMath.h>

#include "CalibCalorimetry/EcalLaserAnalyzer/plugins/EcalABAnalyzer.h"

#include <sstream>
#include <fstream>
#include <iomanip>

#include <FWCore/MessageLogger/interface/MessageLogger.h>
#include <FWCore/Framework/interface/EventSetup.h>
#include <FWCore/Framework/interface/Event.h>
#include <FWCore/Framework/interface/MakerMacros.h>
#include <FWCore/ParameterSet/interface/ParameterSet.h>
#include <DataFormats/EcalDetId/interface/EcalElectronicsId.h>
#include <DataFormats/EcalDetId/interface/EcalDetIdCollections.h>

#include <CalibCalorimetry/EcalLaserAnalyzer/interface/TShapeAnalysis.h>
#include <CalibCalorimetry/EcalLaserAnalyzer/interface/TMom.h>
#include <CalibCalorimetry/EcalLaserAnalyzer/interface/TAPD.h>
#include <CalibCalorimetry/EcalLaserAnalyzer/interface/TAPDPulse.h>
#include <CalibCalorimetry/EcalLaserAnalyzer/interface/PulseFitWithFunction.h>
#include <CalibCalorimetry/EcalLaserAnalyzer/interface/ME.h>
#include <CalibCalorimetry/EcalLaserAnalyzer/interface/MEGeom.h>

using namespace std;

//========================================================================
EcalABAnalyzer::EcalABAnalyzer(const edm::ParameterSet& iConfig)
    //========================================================================
    : iEvent(0),
      eventHeaderCollection_(iConfig.getParameter<std::string>("eventHeaderCollection")),
      eventHeaderProducer_(iConfig.getParameter<std::string>("eventHeaderProducer")),
      digiCollection_(iConfig.getParameter<std::string>("digiCollection")),
      digiProducer_(iConfig.getParameter<std::string>("digiProducer")),
      rawDataToken_(consumes<EcalRawDataCollection>(edm::InputTag(eventHeaderProducer_, eventHeaderCollection_))),
      mappingToken_(esConsumes()),
      // framework parameters with default values
      _nsamples(iConfig.getUntrackedParameter<unsigned int>("nSamples", 10)),
      _presample(iConfig.getUntrackedParameter<unsigned int>("nPresamples", 2)),
      _firstsample(iConfig.getUntrackedParameter<unsigned int>("firstSample", 1)),
      _lastsample(iConfig.getUntrackedParameter<unsigned int>("lastSample", 2)),
      _timingcutlow(iConfig.getUntrackedParameter<unsigned int>("timingCutLow", 2)),
      _timingcuthigh(iConfig.getUntrackedParameter<unsigned int>("timingCutHigh", 9)),
      _timingquallow(iConfig.getUntrackedParameter<unsigned int>("timingQualLow", 3)),
      _timingqualhigh(iConfig.getUntrackedParameter<unsigned int>("timingQualHigh", 8)),
      _ratiomincutlow(iConfig.getUntrackedParameter<double>("ratioMinCutLow", 0.4)),
      _ratiomincuthigh(iConfig.getUntrackedParameter<double>("ratioMinCutHigh", 0.95)),
      _ratiomaxcutlow(iConfig.getUntrackedParameter<double>("ratioMaxCutLow", 0.8)),
      _presamplecut(iConfig.getUntrackedParameter<double>("presampleCut", 5.0)),
      _niter(iConfig.getUntrackedParameter<unsigned int>("nIter", 3)),
      _alpha(iConfig.getUntrackedParameter<double>("alpha", 1.5076494)),
      _beta(iConfig.getUntrackedParameter<double>("beta", 1.5136036)),
      _nevtmax(iConfig.getUntrackedParameter<unsigned int>("nEvtMax", 200)),
      _noise(iConfig.getUntrackedParameter<double>("noise", 2.0)),
      _chi2cut(iConfig.getUntrackedParameter<double>("chi2cut", 100.0)),
      _ecalPart(iConfig.getUntrackedParameter<std::string>("ecalPart", "EB")),
      _fedid(iConfig.getUntrackedParameter<int>("fedId", -999)),
      _qualpercent(iConfig.getUntrackedParameter<double>("qualPercent", 0.2)),
      _debug(iConfig.getUntrackedParameter<int>("debug", 0)),
      resdir_(iConfig.getUntrackedParameter<std::string>("resDir")),
      nCrys(NCRYSEB),
      runType(-1),
      runNum(0),
      fedID(-1),
      dccID(-1),
      side(2),
      lightside(2),
      iZ(1),
      phi(-1),
      eta(-1),
      event(0),
      color(-1),
      channelIteratorEE(0)

//========================================================================

{
  if (_ecalPart == "EB") {
    ebDigiToken_ = consumes<EBDigiCollection>(edm::InputTag(digiProducer_, digiCollection_));
  } else if (_ecalPart == "EE") {
    eeDigiToken_ = consumes<EEDigiCollection>(edm::InputTag(digiProducer_, digiCollection_));
  }

  // Geometrical constants initialization

  if (_ecalPart == "EB") {
    nCrys = NCRYSEB;
  } else {
    nCrys = NCRYSEE;
  }
  iZ = 1;
  if (_fedid <= 609)
    iZ = -1;

  for (unsigned int j = 0; j < nCrys; j++) {
    iEta[j] = -1;
    iPhi[j] = -1;
    iTowerID[j] = -1;
    iChannelID[j] = -1;
    idccID[j] = -1;
    iside[j] = -1;
    wasTimingOK[j] = true;
    wasGainOK[j] = true;
    nevtAB[j] = 0;
  }

  // Quality check flags

  isGainOK = true;
  isTimingOK = true;

  // Objects dealing with pulses

  APDPulse = new TAPDPulse(_nsamples,
                           _presample,
                           _firstsample,
                           _lastsample,
                           _timingcutlow,
                           _timingcuthigh,
                           _timingquallow,
                           _timingqualhigh,
                           _ratiomincutlow,
                           _ratiomincuthigh,
                           _ratiomaxcutlow);

  // Objects needed for npresample calculation

  Delta01 = new TMom();
  Delta12 = new TMom();
  _fitab = true;
}

//========================================================================
EcalABAnalyzer::~EcalABAnalyzer() {
  //========================================================================

  // do anything here that needs to be done at desctruction time
  // (e.g. close files, deallocate resources etc.)
}

//========================================================================
void EcalABAnalyzer::beginJob() {
  //========================================================================

  //Calculate alpha and beta

  // Define output results filenames and shape analyzer object (alpha,beta)
  //=====================================================================

  // 1) AlphaBeta files

  doesABTreeExist = true;

  std::stringstream nameabinitfile;
  nameabinitfile << resdir_ << "/ABInit.root";
  alphainitfile = nameabinitfile.str();

  std::stringstream nameabfile;
  std::stringstream link;
  nameabfile << resdir_ << "/AB.root";

  FILE* test;
  test = fopen(nameabinitfile.str().c_str(), "r");
  if (test == nullptr) {
    doesABTreeExist = false;
    _fitab = true;
  };
  delete test;

  TFile* fAB = nullptr;
  TTree* ABInit = nullptr;
  if (doesABTreeExist) {
    fAB = new TFile(nameabinitfile.str().c_str());
  }
  if (doesABTreeExist && fAB) {
    ABInit = (TTree*)fAB->Get("ABCol0");
  }

  // 2) Shape analyzer

  if (doesABTreeExist && fAB && ABInit && ABInit->GetEntries() != 0) {
    shapana = new TShapeAnalysis(ABInit, _alpha, _beta, 5.5, 1.0);
    doesABTreeExist = true;
  } else {
    shapana = new TShapeAnalysis(_alpha, _beta, 5.5, 1.0);
    doesABTreeExist = false;
    _fitab = true;
  }
  shapana->set_const(_nsamples, _firstsample, _lastsample, _presample, _nevtmax, _noise, _chi2cut);

  if (doesABTreeExist && fAB)
    fAB->Close();

  if (_fitab) {
    alphafile = nameabfile.str();
  } else {
    alphafile = alphainitfile;
    link << "ln -s " << resdir_ << "/ABInit.root " << resdir_ << "/AB.root";
    system(link.str().c_str());
  }

  // Define output results files' names

  std::stringstream namefile;
  namefile << resdir_ << "/AB.root";
  alphafile = namefile.str();
}

//========================================================================
void EcalABAnalyzer::analyze(const edm::Event& e, const edm::EventSetup& c) {
  //========================================================================

  ++iEvent;

  // retrieving DCC header
  edm::Handle<EcalRawDataCollection> pDCCHeader;
  const EcalRawDataCollection* DCCHeader = nullptr;
  e.getByToken(rawDataToken_, pDCCHeader);
  if (!pDCCHeader.isValid()) {
    edm::LogError("nodata") << "Error! can't get the product retrieving DCC header" << eventHeaderCollection_.c_str()
                            << " " << eventHeaderProducer_.c_str();
  } else {
    DCCHeader = pDCCHeader.product();
  }

  //retrieving crystal data from Event
  edm::Handle<EBDigiCollection> pEBDigi;
  const EBDigiCollection* EBDigi = nullptr;
  edm::Handle<EEDigiCollection> pEEDigi;
  const EEDigiCollection* EEDigi = nullptr;
  if (_ecalPart == "EB") {
    e.getByToken(ebDigiToken_, pEBDigi);
    if (!pEBDigi.isValid()) {
      edm::LogError("nodata") << "Error! can't get the product retrieving EB crystal data " << digiCollection_.c_str();
    } else {
      EBDigi = pEBDigi.product();
    }
  } else if (_ecalPart == "EE") {
    e.getByToken(eeDigiToken_, pEEDigi);
    if (!pEEDigi.isValid()) {
      edm::LogError("nodata") << "Error! can't get the product retrieving EE crystal data " << digiCollection_.c_str();
    } else {
      EEDigi = pEEDigi.product();
    }
  } else {
    edm::LogError("cfg_error") << " Wrong ecalPart in cfg file";
    return;
  }

  // retrieving electronics mapping
  const auto& TheMapping = c.getData(mappingToken_);

  // =============================
  // Decode DCCHeader Information
  // =============================

  for (EcalRawDataCollection::const_iterator headerItr = DCCHeader->begin(); headerItr != DCCHeader->end();
       ++headerItr) {
    // Get run type and run number

    int fed = headerItr->fedId();
    if (fed != _fedid && _fedid != -999)
      continue;

    runType = headerItr->getRunType();
    runNum = headerItr->getRunNumber();
    event = headerItr->getLV1();

    dccID = headerItr->getDccInTCCCommand();
    fedID = headerItr->fedId();
    lightside = headerItr->getRtHalf();

    // Check fed corresponds to the DCC in TCC

    if (600 + dccID != fedID)
      continue;

    // Cut on runType

    if (runType != EcalDCCHeaderBlock::LASER_STD && runType != EcalDCCHeaderBlock::LASER_GAP &&
        runType != EcalDCCHeaderBlock::LASER_POWER_SCAN && runType != EcalDCCHeaderBlock::LASER_DELAY_SCAN)
      return;

    // Retrieve laser color and event number

    EcalDCCHeaderBlock::EcalDCCEventSettings settings = headerItr->getEventSettings();
    color = settings.wavelength;
    if (color < 0)
      return;

    std::vector<int>::iterator iter = find(colors.begin(), colors.end(), color);
    if (iter == colors.end()) {
      colors.push_back(color);
    }
  }

  // Cut on fedID

  if (fedID != _fedid && _fedid != -999)
    return;

  // ===========================
  // Decode EBDigis Information
  // ===========================

  int adcGain = 0;

  if (EBDigi) {
    // Loop on crystals
    //===================

    for (EBDigiCollection::const_iterator digiItr = EBDigi->begin(); digiItr != EBDigi->end();
         ++digiItr) {  // Loop on crystals

      // Retrieve geometry
      //===================

      EBDetId id_crystal(digiItr->id());
      EBDataFrame df(*digiItr);

      int etaG = id_crystal.ieta();  // global
      int phiG = id_crystal.iphi();  // global

      int etaL;  // local
      int phiL;  // local
      std::pair<int, int> LocalCoord = MEEBGeom::localCoord(etaG, phiG);

      etaL = LocalCoord.first;
      phiL = LocalCoord.second;

      eta = etaG;
      phi = phiG;

      side = MEEBGeom::side(etaG, phiG);

      // Recover the TT id and the electronic crystal numbering from EcalElectronicsMapping

      EcalElectronicsId elecid_crystal = TheMapping.getElectronicsId(id_crystal);

      int towerID = elecid_crystal.towerId();
      int strip = elecid_crystal.stripId();
      int xtal = elecid_crystal.xtalId();
      int channelID = 5 * (strip - 1) + xtal - 1;

      unsigned int channel = MEEBGeom::electronic_channel(etaL, phiL);

      assert(channel < nCrys);

      iEta[channel] = eta;
      iPhi[channel] = phi;
      iTowerID[channel] = towerID;
      iChannelID[channel] = channelID;
      idccID[channel] = dccID;
      iside[channel] = side;

      // APD Pulse
      //===========

      // Loop on adc samples

      for (unsigned int i = 0; i < (*digiItr).size(); ++i) {
        EcalMGPASample samp_crystal(df.sample(i));
        adc[i] = samp_crystal.adc();
        adcG[i] = samp_crystal.gainId();
        adc[i] *= adcG[i];
        if (i == 0)
          adcGain = adcG[i];
        if (i > 0)
          adcGain = TMath::Max(adcG[i], adcGain);
      }

      APDPulse->setPulse(adc);

      // Quality checks
      //================

      if (adcGain != 1)
        nEvtBadGain[channel]++;
      if (!APDPulse->isTimingQualOK())
        nEvtBadTiming[channel]++;
      nEvtTot[channel]++;

      // Fill if Pulse is fine
      //=======================

      if (APDPulse->isPulseOK() && lightside == side) {
        Delta01->addEntry(APDPulse->getDelta(0, 1));
        Delta12->addEntry(APDPulse->getDelta(1, 2));

        if (nevtAB[channel] < _nevtmax && _fitab) {
          if (doesABTreeExist)
            shapana->putAllVals(channel, adc, eta, phi);
          else
            shapana->putAllVals(channel, adc, eta, phi, dccID, side, towerID, channelID);
          nevtAB[channel]++;
        }
      }
    }

  } else if (EEDigi) {
    // Loop on crystals
    //===================

    for (EEDigiCollection::const_iterator digiItr = EEDigi->begin(); digiItr != EEDigi->end();
         ++digiItr) {  // Loop on crystals

      // Retrieve geometry
      //===================

      EEDetId id_crystal(digiItr->id());
      EEDataFrame df(*digiItr);

      phi = id_crystal.ix();
      eta = id_crystal.iy();

      int iX = (phi - 1) / 5 + 1;
      int iY = (eta - 1) / 5 + 1;

      side = MEEEGeom::side(iX, iY, iZ);
      EcalElectronicsId elecid_crystal = TheMapping.getElectronicsId(id_crystal);

      int towerID = elecid_crystal.towerId();
      int channelID = elecid_crystal.channelId() - 1;

      int hashedIndex = 100000 * eta + phi;

      if (channelMapEE.count(hashedIndex) == 0) {
        channelMapEE[hashedIndex] = channelIteratorEE;
        channelIteratorEE++;
      }

      unsigned int channel = channelMapEE[hashedIndex];

      assert(channel < nCrys);

      iEta[channel] = eta;
      iPhi[channel] = phi;
      iTowerID[channel] = towerID;
      iChannelID[channel] = channelID;
      idccID[channel] = dccID;
      iside[channel] = side;

      // APD Pulse
      //===========

      if ((*digiItr).size() > 10)
        edm::LogVerbatim("EcalABAnalyzer") << "SAMPLES SIZE > 10!" << (*digiItr).size();

      // Loop on adc samples

      for (unsigned int i = 0; i < (*digiItr).size(); ++i) {
        EcalMGPASample samp_crystal(df.sample(i));
        adc[i] = samp_crystal.adc();
        adcG[i] = samp_crystal.gainId();
        adc[i] *= adcG[i];

        if (i == 0)
          adcGain = adcG[i];
        if (i > 0)
          adcGain = TMath::Max(adcG[i], adcGain);
      }

      APDPulse->setPulse(adc);

      // Quality checks
      //================

      if (adcGain != 1)
        nEvtBadGain[channel]++;
      if (!APDPulse->isTimingQualOK())
        nEvtBadTiming[channel]++;
      nEvtTot[channel]++;

      // Fill if Pulse is fine
      //=======================

      if (APDPulse->isPulseOK() && lightside == side) {
        Delta01->addEntry(APDPulse->getDelta(0, 1));
        Delta12->addEntry(APDPulse->getDelta(1, 2));

        if (nevtAB[channel] < _nevtmax && _fitab) {
          if (doesABTreeExist)
            shapana->putAllVals(channel, adc, eta, phi);
          else
            shapana->putAllVals(channel, adc, eta, phi, dccID, side, towerID, channelID);
          nevtAB[channel]++;
        }
      }
    }
  }
}  // analyze

//========================================================================
void EcalABAnalyzer::endJob() {
  //========================================================================

  edm::LogVerbatim("EcalABAnalyzer") << "\n\t+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+";
  edm::LogVerbatim("EcalABAnalyzer") << "\t+=+     Analyzing data: getting (alpha, beta)     +=+";

  // Adjust channel numbers for EE
  //===============================

  if (_ecalPart == "EE") {
    nCrys = channelMapEE.size();
    shapana->set_nch(nCrys);
  }

  // Set presamples number
  //======================

  double delta01 = Delta01->getMean();
  double delta12 = Delta12->getMean();
  if (delta12 > _presamplecut) {
    _presample = 2;
    if (delta01 > _presamplecut)
      _presample = 1;
  }

  APDPulse->setPresamples(_presample);
  shapana->set_presample(_presample);

  //  Get alpha and beta
  //======================

  if (_fitab) {
    edm::LogVerbatim("EcalABAnalyzer") << "\n\t+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+";
    edm::LogVerbatim("EcalABAnalyzer") << "\t+=+     Analyzing data: getting (alpha, beta)     +=+";
    TFile* fAB = nullptr;
    TTree* ABInit = nullptr;
    if (doesABTreeExist) {
      fAB = new TFile(alphainitfile.c_str());
    }
    if (doesABTreeExist && fAB) {
      ABInit = (TTree*)fAB->Get("ABCol0");
    }
    shapana->computeShape(alphafile, ABInit);

    // Set quality flags for gains and timing

    double BadGainEvtPercentage = 0.0;
    double BadTimingEvtPercentage = 0.0;

    int nChanBadGain = 0;
    int nChanBadTiming = 0;

    for (unsigned int i = 0; i < nCrys; i++) {
      if (nEvtTot[i] != 0) {
        BadGainEvtPercentage = double(nEvtBadGain[i]) / double(nEvtTot[i]);
        BadTimingEvtPercentage = double(nEvtBadTiming[i]) / double(nEvtTot[i]);
      }
      if (BadGainEvtPercentage > _qualpercent) {
        wasGainOK[i] = false;
        nChanBadGain++;
      }
      if (BadTimingEvtPercentage > _qualpercent) {
        wasTimingOK[i] = false;
        nChanBadTiming++;
      }
    }

    double BadGainChanPercentage = double(nChanBadGain) / double(nCrys);
    double BadTimingChanPercentage = double(nChanBadTiming) / double(nCrys);

    if (BadGainChanPercentage > _qualpercent)
      isGainOK = false;
    if (BadTimingChanPercentage > _qualpercent)
      isTimingOK = false;

    if (!isGainOK)
      edm::LogVerbatim("EcalABAnalyzer") << "\t+=+ ............................ WARNING! APD GAIN WAS NOT 1    +=+";
    if (!isTimingOK)
      edm::LogVerbatim("EcalABAnalyzer") << "\t+=+ ............................ WARNING! TIMING WAS BAD        +=+";

    edm::LogVerbatim("EcalABAnalyzer") << "\t+=+    .................................... done  +=+";
    edm::LogVerbatim("EcalABAnalyzer") << "\t+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+";
  }
}

DEFINE_FWK_MODULE(EcalABAnalyzer);
