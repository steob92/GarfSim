#include <iostream>
#include <cmath>
#include <TCanvas.h>
#include <TGraph.h>
#include <TF1.h>
#include "DCsim.hh"
#include "TMath.h"
#include "TVirtualFFT.h"
#include "TComplex.h"
#include "TGraphErrors.h"
//#include "TGraph.h "

DCsim::DCsim(TRandom3 r)
  :
      m_className("ViewCell"),
      m_debug(true),
      digitPeriod(10),
      histbin(2),
      waveformLength(1000),
      nwires(0),
      gasGain(1e5),
      transimpedanceGain(20000.0),
      fullscaleADC(4096),
      fullscaleVoltage(-3.0),  // z=10   3V full scale
      ADCthresh(2),
      nedgesamples(10),
      noiseRMS(0.7e-3),   // rms output noise (Volts) 
      firstedgesample(0)
  
{rloc = r;
 }

DCsim::~DCsim() {

}

void DCsim::Copy(DCsim * insim){
}


void DCsim::MakeTree(char * rootfile){
   outfile = new TFile(rootfile,"RECREATE");
   DCtree = new TTree("t","");
   DCtree->Branch("Int1",&int1,"Int1/d");
   DCtree->Branch("Int2",&int2,"Int2/d");
   DCtree->Branch("Dif1",&dif1,"Dif1/d");
   DCtree->Branch("trackX",&trackstartx,"trackX/f");
   DCtree->Branch("trackY",&trackstarty,"trackY/f");
   DCtree->Branch("trackSlope",&trackangle,"trackSlope/f");
   DCtree->Branch("tthresh",tthresh,"tthresh[7]/f");
   DCtree->Branch("tfit",&tedgefit,"tfit[7]/f");
}

void DCsim::SetNwires(int iw){
  nwires = iw;
}

void DCsim::SetWirePos(int iw,float x,float y){
  wireStruct wp;
  wp.x=x;
  wp.y=y;
  wireInfo.push_back(wp);
}

void DCsim::InitDigitization(){
  for(int iwire=0;iwire<nwires;iwire++){
    wireSig * wiresig = new wireSig();
    for (int ndigit = 0 ; ndigit<waveformLength; ndigit++){
      wiresig->Sample.push_back(0);
    }
    DCDigitization.push_back(wiresig);
  }
}

void DCsim::Digitize(int iw,  TH1D* hist){
  for (int idig=0;idig<waveformLength;idig++){
    int binperdig = int(digitPeriod/histbin);
    double sample = hist->GetBinContent(idig*binperdig);
    DCDigitization.at(iw)->Sample.at(idig)=fullscaleADC*(sample/fullscaleVoltage);
  }
}

void DCsim::Digitize(TH1D* rawWaveform, TH1D* digitizedWaveform){
  digitizedWaveform->Reset();
  for (int idig=1;idig<waveformLength;idig++){
    double sample = rawWaveform->GetBinContent(idig);
    digitizedWaveform->SetBinContent(idig,fullscaleADC*(sample/fullscaleVoltage));
  }
}

void DCsim::CreateNoise(TH1D* NoiseWave){
  NoiseWave->Reset();
  int maxbin =NoiseWave->GetNbinsX();
  for (int idig=1;idig<maxbin;idig++){
    NoiseWave->SetBinContent(idig,rloc.Gaus(0,1));
  }

  TH1 *hm =0;
  TVirtualFFT::SetTransform(0);
  hm = NoiseWave->FFT(hm, "MAG");
  hm->SetTitle("Magnitude of the 1st transform");

  //  TH1D * FlatFreq = new TH1D("FlatFreq","FlatFreq",maxbin,0,maxbin/10e-6);
  //for(int ibin=0;ibin<maxbin;ibin++){
  //  FlatFreq->SetBinContent(ibin,hm->GetBinContent(ibin)/TMath::Sqrt(maxbin));
  //}
  //  FlatFreq->Write();

  //NOTE: for "real" frequencies you have to divide the x-axes range with the range of your function
  //(in this case 10usec); y-axes has to be rescaled by a factor of 1/SQRT(5000) to be right: this is not done automatically!

//That's the way to get the current transform object:
  TVirtualFFT *fft = TVirtualFFT::GetCurrentTransform();
 
//Use the following method to get the full output:
  Double_t *re_full = new Double_t[maxbin];
  Double_t *im_full = new Double_t[maxbin];
  Double_t re_i;
  Double_t im_i;
  TH1D * NoiseDensity = new TH1D("NoiseDensity","NoiseDensity",maxbin,0,maxbin/10e-6);
  
  for(int ibin=0;ibin<maxbin;ibin++){
    double freq = (ibin/10e-6);
    double noise = AmpVoltageNoise(freq);
    fft->GetPointComplex(ibin,re_i,im_i);
    re_full[ibin]=re_i*noise;   im_full[ibin]=im_i*noise;
    NoiseDensity->SetBinContent(ibin,noise*TMath::Sqrt(re_i*re_i+im_i*im_i));
  }
 
  // NoiseDensity->Write();

  //Now let's make a backward transform:

  NoiseWave->Reset();
  TVirtualFFT *fft_back = TVirtualFFT::FFT(1, &maxbin, "C2R");
  fft_back->SetPointsComplex(re_full,im_full);
  fft_back->Transform();
  TH1 *hb = 0;
  //Let's look at the output
  hb = TH1::TransformHisto(fft_back,hb,"Re");
  hb->SetTitle("The backward transform result");
 
  // calc RMS and rescale to get correct RMS output noise.
  double TotalRMSNoise=AmpVoltageNoise(0);
  
  double ave2=0;
  double ave=0;

  for(int ibin=0;ibin<maxbin;ibin++){
    double val =hb->GetBinContent(ibin);
    ave += val;
    ave2 += val*val;
  }

  double rms = TMath::Sqrt((ave2-ave*ave/maxbin)/(maxbin-1));
  std::cout << "pre-scaled rms " << rms*1e3 << " mV" << std::endl;

  //  TH1D * OutNoise = new TH1D("OutNoise","OutNoise",maxbin,0,10e-6);
  for(int ibin=0;ibin<maxbin;ibin++){
    NoiseWave->SetBinContent(ibin,hb->GetBinContent(ibin)*TotalRMSNoise/rms);
  }
  // NoiseWave->Write();
  
  ave=0;
  ave2=0;
  for(int ibin=0;ibin<maxbin;ibin++){
    double val =NoiseWave->GetBinContent(ibin);
    ave += val;
    ave2 += val*val;
  }
  rms = TMath::Sqrt((ave2-ave*ave/maxbin)/(maxbin-1));
  std::cout << " final rms " << rms*1E3 << " mV" << std::endl;

  //  hm =0;
  // TVirtualFFT::SetTransform(0);
  //hm = NoiseWave->FFT(hm, "MAG");
  //hm->SetTitle("Magnitude of the 1st transform");
  //  hm->Print();
  ///  TH1D * ColorFreq = new TH1D("ColorFreq","ColorFreq",maxbin,0,maxbin/10e-6);
  // for(int ibin=0;ibin<maxbin;ibin++){
  //  ColorFreq->SetBinContent(ibin,hm->GetBinContent(ibin)/TMath::Sqrt(maxbin));
  // }
  //  ColorFreq->Write();

  
  //NOTE: here you get at the x-axes number of bins and not real values
  //(in this case 25 bins has to be rescaled to a range between 0 and 4*Pi;
  //also here the y-axes has to be rescaled (factor 1/bins)
  delete[] re_full;
  delete[] im_full;
  delete NoiseDensity;
}

double DCsim::AmpVoltageNoise(double freq){
  double V_rf = Vrf;
  double V_series = Vin;
  double V_parallel = Ven;
  double V_det=Vdet;

  if(freq>F_0){
    V_rf=V_rf*F_0/freq;
    V_parallel=V_parallel*F_0/freq;
    V_det=V_det*F_0/freq;
    V_series = Vin*(F_0/F_z)*(F_0/freq);
  }
  if(freq>F_z && freq<=F_0){
    V_series = Vin *freq/F_z;
  }
  return TMath::Sqrt(V_rf*V_rf+V_parallel*V_parallel+V_series*V_series+V_det*V_det);
}

void DCsim::Filter(TH1D* rawWave, TH1D* filteredWave, TH1D * Diff1Sig, TH1D* Int2Sig, double Int1timeconstant, double Diff1timeconstant, double Int2timeconstant){

  int1=Int1timeconstant;
  int2=Int2timeconstant;
  dif1=Diff1timeconstant;

  // simple single pole low pass filter;
  //  double timeconstant = 10; //ns
  Diff1Sig->Reset(); 
  filteredWave->Reset();
  Int2Sig->Reset();

  
  int nbins = rawWave->GetNbinsX();
  double filtered=0;
  if(Int1timeconstant>0){
    for(int ibin=1;ibin<nbins;ibin++){
      filtered=0;
      // detector/premap integration
      for(int rbin=1;rbin<=ibin;rbin++){
	double deltat = (ibin-rbin)*histbin;
	filtered += rawWave->GetBinContent(rbin)*TMath::Exp(-deltat/Int1timeconstant)*(histbin/Int1timeconstant);
      }
      filteredWave->SetBinContent(ibin,filtered);
    }
  }
  else{
    for(int ibin=1;ibin<nbins;ibin++){
      filteredWave->SetBinContent(ibin,rawWave->GetBinContent(ibin));
    }
  }
    // first differentiation
  //  double Diff1timeconstant = 100; //ns
  if(Diff1timeconstant>0){
    for(int ibin=1;ibin<nbins;ibin++){
      filtered=0;
      // detector/premap integration
      for(int rbin=2;rbin<=ibin;rbin++){
	double deltat = (ibin-rbin)*histbin;
	filtered += (filteredWave->GetBinContent(rbin)-filteredWave->GetBinContent(rbin-1))*TMath::Exp(-deltat/Diff1timeconstant);
      }
      Diff1Sig->SetBinContent(ibin,filtered);
    }
  }
  else{
    for(int ibin=1;ibin<nbins;ibin++){
      Diff1Sig->SetBinContent(ibin,filteredWave->GetBinContent(ibin));
    }
  }

    // 2nd Integration
  //  double Int2timeconstant = 50; //ns
  if(Int2timeconstant>0){
    for(int ibin=1;ibin<nbins;ibin++){
      filtered=0;
      // detector/premap integration
      for(int rbin=1;rbin<=ibin;rbin++){
	double deltat = (ibin-rbin)*histbin;
	filtered += (Diff1Sig->GetBinContent(rbin))*TMath::Exp(-deltat/Int2timeconstant)*(histbin/Int2timeconstant);
      }
      Int2Sig->SetBinContent(ibin,filtered);
    }
  }
  else{
    for(int ibin=1;ibin<nbins;ibin++){
      Int2Sig->SetBinContent(ibin,Diff1Sig->GetBinContent(ibin));
    }
  }
}

void DCsim::ApplyGainandNoise(TH1D* InCurrent, TH1D* NoiseWave, TH1D* OutVoltage){
  int nbins = InCurrent->GetNbinsX();
  for(int ibin=1;ibin<nbins;ibin++){
    OutVoltage->SetBinContent(ibin,InCurrent->GetBinContent(ibin)*1e-6*gasGain*transimpedanceGain
      + NoiseWave->GetBinContent(ibin));
  }
}

void DCsim::AddNoise(TH1D* InVoltWave,TH1D* OutSig){  // add white noise - superseeded.
  int nbins = InVoltWave->GetNbinsX();
  for(int ibin=1;ibin<nbins;ibin++){
    OutSig->SetBinContent(ibin,InVoltWave->GetBinContent(ibin)+rloc.Gaus(0,noiseRMS));
  }
}

double DCsim::GetWeightingIntegralI1(TH1D* Int2Sig, double Tf){
  double maxval = Int2Sig->GetBinContent(Int2Sig->GetMaximumBin());
  int nbins = Int2Sig->GetNbinsX();
  double a_F1=0;
  for(int ibin=1;ibin<nbins;ibin++){
    a_F1 += (Int2Sig->GetBinContent(ibin)-Int2Sig->GetBinContent(ibin-1)) *(Int2Sig->GetBinContent(ibin)-Int2Sig->GetBinContent(ibin-1))/(histbin*maxval*maxval);
  }
  a_F1 *=Tf;
  return a_F1;
}

double DCsim::GetWeightingIntegralI2(TH1D* Int2Sig, double Tf){
  double maxval = Int2Sig->GetBinContent(Int2Sig->GetMaximumBin());
  int nbins = Int2Sig->GetNbinsX();
  double a_F2=0;
  for(int ibin=1;ibin<nbins;ibin++){
    a_F2 += (Int2Sig->GetBinContent(ibin)*Int2Sig->GetBinContent(ibin))*(histbin/(maxval*maxval));
  }
  a_F2 /=Tf;
  return a_F2;
}

double DCsim::GetBallisticDeficit(TH1D* PreampOut,TH1D * ShaperOut){
  double max_in =  PreampOut->GetBinContent(PreampOut->GetMinimumBin());
  double max_out = ShaperOut->GetBinContent(ShaperOut->GetMinimumBin());
  return max_out/max_in;
}

int DCsim::FirstThresholdPreDig(TH1D* Waveform){
  double thresh = fullscaleVoltage*float(ADCthresh/fullscaleADC);
  std::cout << " looking for crossing below " << thresh*1e3 << "  mV" << std::endl;
  int nbins = Waveform->GetNbinsX();
  for(int ibin=0;ibin<nbins;ibin++){
    if(Waveform->GetBinContent(ibin)<thresh){
      std::cout << " threshold crossing at bin " << ibin << std::endl;
      return ibin;
    }
  }
  return -1;
}

double DCsim::GetPreSampleRMS(TH1D* Waveform, int threshbin, int nsamples){
  double ave=0;
  double ave2=0;
  for(int ibin=threshbin;ibin<nsamples;ibin++){
    double val =Waveform->GetBinContent(ibin);
    //    std::cout << " presample " << ibin << " contents " << val << " mV " << std::endl;
    ave += val;
    ave2 += val*val;
  }
  double rms = TMath::Sqrt((ave2-ave*ave/nsamples)/(nsamples-1));
  std::cout << " baseline rms = " << rms*1e3 << " mV" << std::endl;
  return rms;
}

double DCsim::GetLeadingEdgeSlope(TH1D* Waveform, double rms, int threshbin, int nsamples){
  int nbins = Waveform->GetNbinsX();
  double bininterval = Waveform->GetXaxis()->GetBinCenter(nbins)/(float)nbins;
  double *t = new double[nsamples];
  double *ampl = new double[nsamples];
  double *e = new double[nsamples];
  //  TF1 * mypol1 = new TF1("mypol1","[0]+x*[1]");
  //  mypol1->SetParameters(-0.01,1e-5);

  for(int ibin=threshbin;ibin<threshbin+nsamples;ibin++){
    ampl[ibin-threshbin] =-Waveform->GetBinContent(ibin);
    t[ibin-threshbin]=ibin*bininterval;
    e[ibin-threshbin]=rms;
    std::cout << " leading edge point " << ampl[ibin-threshbin] << " " << t[ibin-threshbin] << std::endl;
  }

 
  TGraphErrors *gr   = new TGraphErrors(nsamples, t,ampl,0,e);
  gr->Fit("pol1");
  //Access the fit resuts
  TF1 *f3 = gr->GetFunction("pol1");
  double slope = f3->GetParameter(1);

  delete[] t;
  delete[] ampl;
  delete[] e;
  // gr->Delete();
  delete gr;

  return slope;

}

int DCsim::GetSample(int iw,int isample){
  return DCDigitization.at(iw)->Sample[isample];
}

 double  DCsim::GetSample(int isample, TH1D* digitizedWaveform){
   return digitizedWaveform->GetBinContent(isample);
}

int DCsim::FirstThreshold(int iw){
  for (int idig=0;idig<waveformLength;idig++){
    if(DCDigitization.at(iw)->Sample[idig]>ADCthresh){
      tthresh[iw]=idig;
      return idig;
    }
  }
  return -1;
}

int DCsim::FirstThreshold(int iw, TH1D * digitizedWaveform){
  for (int idig=1;idig<digitizedWaveform->GetNbinsX()-1;idig++){
    if(digitizedWaveform->GetBinContent(idig)>ADCthresh){
      tthresh[iw]=idig;
      return idig;
    }
  }
  return -1;
}

void DCsim::FitLeadingEdges(){
  double x[1000];
  double y[1000];
  for(int iwire=0;iwire<nwires;iwire++){
    for(int isample=0;isample<nedgesamples;isample++){
	y[isample]=DCDataPacket[iwire].threshbin + isample + firstedgesample;
	x[isample]=DCDataPacket[iwire].Sample[isample+firstedgesample];
    }
    TGraph g(nedgesamples,x,y);
    TF1 * mypol1 = new TF1("mypol1","[0]+x*[1]");
    mypol1->SetParameters(0,1);

    g.Fit("mypol1","Q");
    TF1 * fit = g.GetFunction("mypol1");
    double tfit = fit->GetParameter(0);
    tedgefit[iwire]=tfit;
    if(m_debug) std::cout << iwire << " fit interecept " << tfit << std::endl;

    //   mypol1->Delete();
    delete mypol1;
  }
}

void DCsim::FitLeadingEdges(int iwire ,TH1D* digitizedWaveform){
  double x[1000];
  double y[1000];
  
  for(int isample=0;isample<nedgesamples;isample++){
    y[isample]=tthresh[iwire] + isample + firstedgesample;
    x[isample]=digitizedWaveform->GetBinContent((int)y[isample]);
  }
  TGraph g(nedgesamples,x,y);
  TF1 * mypol1 = new TF1("mypol1","[0]+x*[1]");
  mypol1->SetParameters(-0.01,1e-5);

  g.Fit("mypol1","Q");
  TF1 * fit = g.GetFunction("mypol1");
  double tfit = fit->GetParameter(0);
  tedgefit[iwire]=tfit;
  if(m_debug) std::cout << iwire << " fit interecept " << tfit << std::endl;

  //mypol1->Delete();
  delete mypol1;
}

void DCsim::FillTree(double trackx, double tracky, double trackang){
  trackstartx=trackx;
  trackstarty=tracky;
  trackangle=trackang;
  DCtree->Fill();
}

void DCsim::WriteTree(){
  outfile->cd();
  outfile->Write();

}

