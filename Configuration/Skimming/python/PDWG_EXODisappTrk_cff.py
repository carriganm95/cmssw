import FWCore.ParameterSet.Config as cms

# Unprescale HLT_MET and HLT_SinglePhoton triggers
import HLTrigger.HLTfilters.hltHighLevel_cfi
hltDisappTrk = HLTrigger.HLTfilters.hltHighLevel_cfi.hltHighLevel.clone()
hltDisappTrk.TriggerResultsTag = cms.InputTag( "TriggerResults", "", "HLT" )
hltDisappTrk.HLTPaths = cms.vstring(
    #2016
    "HLT_Photon175_v*",
    "HLT_DoublePhoton60_v*",
    "HLT_PFMET300_v*",
    "HLT_PFMET170_HBHE_BeamHaloCleaned_v*",
    #2017 and 2018
    "HLT_Photon200_v*",
    "HLT_Photon300_NoHE_v*",
    "HLT_DoublePhoton70_v*",
    "HLT_PFMET140_PFMHT140_IDTight_v*",
    "HLT_PFMET250_HBHECleaned_v*",
    "HLT_PFMET300_HBHECleaned_v*"
)
hltDisappTrk.throw = False
hltDisappTrk.andOr = True

from Configuration.EventContent.EventContent_cff import AODEventContent
EXODisappTrkSkimContent = AODEventContent.clone()
EXODisappTrkSkimContent.outputCommands.append('keep *_hybridSuperClusters_*_*')
EXODisappTrkSkimContent.outputCommands.append('keep *_multi5x5SuperClusters_multi5x5EndcapSuperClusters_*')
EXODisappTrkSkimContent.outputCommands.append('keep *_multi5x5SuperClusters_uncleanOnlyMulti5x5EndcapBasicClusters_*')
EXODisappTrkSkimContent.outputCommands.append('keep *_multi5x5SuperClusters_uncleanOnlyMulti5x5EndcapSuperClusters_*')
EXODisappTrkSkimContent.outputCommands.append('keep *_siStripClusters_*_*')
EXODisappTrkSkimContent.outputCommands.append('keep *_siPixelClusters_*_*')
EXODisappTrkSkimContent.outputCommands.append('drop *_generalTracks_*_*')
EXODisappTrkSkimContent.outputCommands.append('keep *_generalTracks_*_*')
EXODisappTrkSkimContent.outputCommands.append('drop *_generalTracks_QualityMasks_*')
EXODisappTrkSkimContent.outputCommands.append('keep *_ecalRecHit_EcalRecHitsEB_*')
EXODisappTrkSkimContent.outputCommands.append('keep *_ecalRecHit_EcalRecHitsEE_*')
EXODisappTrkSkimContent.outputCommands.append('keep *_hbhereco_*_*')

# monopole skim sequence
EXODisappTrkSkimSequence = cms.Sequence(
    hltDisappTrk
    )
