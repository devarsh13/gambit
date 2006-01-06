//
// $Source$
// $Date$
// $Revision$
//
// DESCRIPTION:
// Compute Nash equilibria via simplicial subdivision on the normal form
//
// This file is part of Gambit
// Copyright (c) 2002, The Gambit Project
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//

#include <unistd.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include "libgambit/libgambit.h"

//
// simpdiv is a simplicial subdivision algorithm with restart, for finding
// mixed strategy solutions to general finite n-person games.  It is based on
// van Der Laan, Talman and van Der Heyden, Math in Oper Res, 1987.
//
// The program accepts a normal form game file on standard input.
//
// The program accepts the following command-line options:
// -v:  Verbose output.  Lists the profile computed at each stage of the
//      mesh refinement.  Default is off, in which case the program only
//      prints the (approximate) Nash equilibrium of the last refinement.
//
// -s <filename>: A list of strategy profiles to use as starting points
//
// -f:  Print profiles in floating-point (even though the algorithm inherently
//      computes using rational numbers)
//
// -d #:  Number of decimals to show in floating-point output
//        (Only has an effect in conjunction with -f)
//
// -r #:  Generate random starting points with denominator #.
//
// -n #:  Stop after # equilibria (only effective with -r)
//
// -g #:  Multiplier for grid restart (default is 2)
//
// 
// Some history:
// 
// Original code was written by R. McKelvey.  The implementation was
// kludged to conform to the new standard algorithm interface (namely,
// by removing passing a reference to the game in the ctor); truly,
// the algorithm needs recoded from the ground up to remove gotos and
// simplify things. (TLT, 6/2002)
//


//
// Global options
//

bool g_verbose = false;
bool g_useFloat = false;
int g_numDecimals = 6;
int g_gridResize = 2;

class nfgSimpdiv {
private:
  int m_nRestarts, m_leashLength;

  int t, ibar;
  gbtRational d,pay,maxz,bestz;

  gbtRational Simplex(Gambit::MixedStrategyProfile<gbtRational> &);
  gbtRational getlabel(Gambit::MixedStrategyProfile<gbtRational> &yy, gbtArray<int> &, gbtPVector<gbtRational> &);
  void update(Gambit::RectArray<int> &, Gambit::RectArray<int> &, gbtPVector<gbtRational> &,
	      const gbtPVector<int> &, int j, int i);
  void getY(Gambit::MixedStrategyProfile<gbtRational> &x, gbtPVector<gbtRational> &, 
	    const gbtPVector<int> &, const gbtPVector<int> &, 
	    const gbtPVector<gbtRational> &, const Gambit::RectArray<int> &, int k);
  void getnexty(Gambit::MixedStrategyProfile<gbtRational> &x, const Gambit::RectArray<int> &,
		const gbtPVector<int> &, int i);
  int get_c(int j, int h, int nstrats, const gbtPVector<int> &);
  int get_b(int j, int h, int nstrats, const gbtPVector<int> &);
  
public:
  nfgSimpdiv(void);
  virtual ~nfgSimpdiv();

  int NumRestarts(void) const { return m_nRestarts; }
  void SetNumRestarts(int p_nRestarts) { m_nRestarts = p_nRestarts; }

  int LeashLength(void) const { return m_leashLength; }
  void SetLeashLength(int p_leashLength) { m_leashLength = p_leashLength; }

  void Solve(const Gambit::Game &, const Gambit::MixedStrategyProfile<gbtRational> &);
};


//-------------------------------------------------------------------------
//               nfgSimpdiv: Constructor and destructor
//-------------------------------------------------------------------------

nfgSimpdiv::nfgSimpdiv(void)
  : m_nRestarts(20), m_leashLength(0)
{
  t = 0;
  ibar = 1;
}

nfgSimpdiv::~nfgSimpdiv()
{ }


//-------------------------------------------------------------------------
//               nfgSimpdiv: Private member functions
//-------------------------------------------------------------------------

gbtRational nfgSimpdiv::Simplex(Gambit::MixedStrategyProfile<gbtRational> &y)
{
  gbtArray<int> ylabel(2);
  Gambit::RectArray<int> labels(y.Length(), 2), pi(y.Length(), 2);
  gbtPVector<int> U(y.GetSupport().NumStrats()), TT(y.GetSupport().NumStrats());
  gbtPVector<gbtRational> ab(y.GetSupport().NumStrats());
  gbtPVector<gbtRational> besty(y.GetSupport().NumStrats());
  gbtPVector<gbtRational> v(y);
  besty = y;

  int i = 0;
  int j, k, h, jj, hh,ii, kk,tot;
  gbtRational maxz;

// Label step0 not currently used, hence commented
// step0:;
  ibar=1;
  t=0;
  for(j=1;j<=y.GetGame()->NumPlayers();j++)
    {
      for(h=1;h<=y.GetSupport().NumStrats(j);h++)
	{
	  TT(j,h)=0;
	  U(j,h)=0;
	  if(v(j,h)==gbtRational(0))U(j,h)=1;
	  ab(j,h)=gbtRational(0);
	  y(j,h)=v(j,h);
	}
    }
  
 step1:;
  
  maxz=getlabel(y, ylabel, besty);
  j=ylabel[1];
  h=ylabel[2];
  labels(ibar,1)=j;
  labels(ibar,2)=h;
  
// Label case1a not currently used, hence commented
// case1a:;
  if(TT(j,h)==0 && U(j,h)==0)
    {
      for(hh=1,tot=0;hh<=y.GetSupport().NumStrats(j);hh++)
	if(TT(j,hh)==1 || U(j,hh)==1)tot++;
      if(tot==y.GetSupport().NumStrats(j)-1)goto end;
      else {
	i=t+1;
	goto step2;
      }
    }
      /* case1b */
  else if(TT(j,h))
    {
      i=1;
      while(labels(i,1)!=j || labels(i,2)!=h || i==ibar) i++;
      goto step3;
    }
      /* case1c */
  else if(U(j,h))
    {
      k=h;
      while(U(j,k)){k++;if(k>y.GetSupport().NumStrats(j))k=1;}
      if(TT(j,k)==0)i=t+1;
      else {
	i=1;
	while((pi(i,1)!=j || pi(i,2)!=k) && i<=t)i++;
      }
      goto step2;
    }
  
 step2:;
  getY(y,v,U,TT,ab,pi,i);
  pi.RotateDown(i,t+1);
  pi(i,1)=j;
  pi(i,2)=h;
  labels.RotateDown(i+1,t+2);
  ibar=i+1;
  t++;
  getnexty(y,pi,U,i);
  TT(j,h)=1;
  U(j,h)=0;
  goto step1;
  
 step3:;
  if(i==t+1)ii=t;
  else ii=i;
  j=pi(ii,1);
  h=pi(ii,2);
  k=h;
  if(i<t+1)k=get_b(j,h,y.GetSupport().NumStrats(j),U);
  kk=get_c(j,h,y.GetSupport().NumStrats(j),U);
  if(i==1)ii=t+1;
  else if(i==t+1)ii=1;
  else ii=i-1;
  getY(y,v,U,TT,ab,pi,ii);
  
      /* case3a */
  if(i==1 && (y(j,k)<=gbtRational(0) || 
	      (v(j,k)-y(j,k))>=(gbtRational(m_leashLength))*d)) {
    for(hh=1,tot=0;hh<=y.GetSupport().NumStrats(j);hh++)
      if(TT(j,hh)==1 || U(j,hh)==1)tot++;
    if(tot==y.GetSupport().NumStrats(j)-1) {
      U(j,k)=1;
      goto end;
    }
    else {
      update(pi,labels,ab,U,j,i);
      U(j,k)=1;
      getnexty(y,pi,U,t);
      goto step1;
    }
  }
      /* case3b */
  else if(i>=2 && i<=t &&
	  (y(j,k)<=gbtRational(0) || (v(j,k)-y(j,k))>=(gbtRational(m_leashLength))*d)) {
    goto step4;
  }
      /* case3c */
  else if(i==t+1 && ab(j,kk)==gbtRational(0)) {
    if(y(j,h)<=gbtRational(0) || (v(j,h)-y(j,h))>=(gbtRational(m_leashLength))*d)goto step4;
    else {
      k=0;
      while(ab(j,kk)==gbtRational(0) && k==0) {
	if(kk==h)k=1;
	kk++;
	if(kk>y.GetSupport().NumStrats(j))kk=1;
      }
      kk--;
      if(kk==0)kk=y.GetSupport().NumStrats(j);
      if(kk==h)goto step4;
      else goto step5;
    }
  }
  else {
      if(i==1) getnexty(y,pi,U,1);
      else if(i<=t) getnexty(y,pi,U,i);
      else if(i==t+1) {
	j=pi(t,1);
	h=pi(t,2);
	hh=get_b(j,h,y.GetSupport().NumStrats(j),U);
	y(j,h)-=d;
	y(j,hh)+=d;
      }
      update(pi,labels,ab,U,j,i);
    }
  goto step1;
 step4:;
  getY(y,v,U,TT,ab,pi,1);
  j=pi(i-1,1);
  h=pi(i-1,2);
  TT(j,h)=0;
  if(y(j,h)<=gbtRational(0) || (v(j,h)-y(j,h))>=(gbtRational(m_leashLength))*d)U(j,h)=1;
  labels.RotateUp(i,t+1);
  pi.RotateUp(i-1,t);
  t--;
  ii=1;
  while(labels(ii,1)!=j || labels(ii,2)!=h) {ii++;}
  i=ii;
  goto step3;
 step5:;
  k=kk;

  labels.RotateDown(1,t+1);
  ibar=1;
  pi.RotateDown(1,t);
  U(j,k)=0;
  jj=pi(1,1);
  hh=pi(1,2);
  kk=get_b(jj,hh,y.GetSupport().NumStrats(jj),U);
  y(jj,hh)-=d;
  y(jj,kk)+=d;
  
  k=get_c(j,h,y.GetSupport().NumStrats(j),U);
  kk=1;
  while(kk){
    if(k==h)kk=0;
    ab(j,k)=(ab(j,k)-(gbtRational(1)));
    k++;
    if(k>y.GetSupport().NumStrats(j))k=1;
  }
  goto step1;

 end:;
  maxz=bestz;
  for(i=1;i<=y.GetGame()->NumPlayers();i++)
    for(j=1;j<=y.GetSupport().NumStrats(i);j++)
      y(i,j)=besty(i,j);
  return maxz;
}

void nfgSimpdiv::update(Gambit::RectArray<int> &pi,
			Gambit::RectArray<int> &labels,
			gbtPVector<gbtRational> &ab,
			const gbtPVector<int> &U,
			int j, int i)
{
  int jj, hh, k,f;
  
  f=1;
  if(i>=2 && i<=t) {
    pi.SwitchRows(i,i-1);
    ibar=i;
  }
  else if(i==1) {
    labels.RotateUp(1,t+1);
    ibar=t+1;
    jj=pi(1,1);
    hh=pi(1,2);
    if(jj==j) {
      k=get_c(jj,hh,ab.Lengths()[jj],U);
      while(f) {
	if(k==hh)f=0;
	ab(j,k)=ab(j,k) + (gbtRational(1));
	k++;
	if(k>ab.Lengths()[jj])k=1;
      }
      pi.RotateUp(1,t);
    }
  }
  else if(i==t+1) {
    labels.RotateDown(1,t+1);
    ibar=1;
    jj=pi(t,1);
    hh=pi(t,2);
    if(jj==j) {
      k=get_c(jj,hh,ab.Lengths()[jj],U);
      while(f) {
	if(k==hh)f=0;
	ab(j,k)= ab(j,k)-(gbtRational(1));
	k++;
	if(k>ab.Lengths()[jj])k=1;
      }
      pi.RotateDown(1,t);
    }
  }
}

void nfgSimpdiv::getY(Gambit::MixedStrategyProfile<gbtRational> &x,
		      gbtPVector<gbtRational> &v, 
		      const gbtPVector<int> &U,
		      const gbtPVector<int> &TT,
		      const gbtPVector<gbtRational> &ab,
		      const Gambit::RectArray<int> &pi,
		      int k)
{
  int j, h, i,hh;
  
  for(j=1;j<=x.GetGame()->NumPlayers();j++)
    for(h=1;h<=x.GetSupport().NumStrats(j);h++)
      x(j,h)=v(j,h);
  for(j=1;j<=x.GetGame()->NumPlayers();j++)
    for(h=1;h<=x.GetSupport().NumStrats(j);h++)
      if(TT(j,h)==1 || U(j,h)==1) {
	x(j,h)+=(d*ab(j,h));
	hh=h-1;
	if(hh==0)hh=x.GetSupport().NumStrats(j);
	x(j,hh)-=(d*ab(j,h));
      }
  i=2;
  while(i<=k) {
    getnexty(x,pi,U,i-1);
    i++;
  }
}

void nfgSimpdiv::getnexty(Gambit::MixedStrategyProfile<gbtRational> &x,
			  const Gambit::RectArray<int> &pi, 
			  const gbtPVector<int> &U,
			  int i)
{
  int j,h,hh;
  
  assert(i>=1);
  j=pi(i,1);
  h=pi(i,2);
  x(j,h)+=d;
  hh=get_b(j,h,x.GetSupport().NumStrats(j),U);
  x(j,hh)-=d;
}

int nfgSimpdiv::get_b(int j, int h, int nstrats,
		      const gbtPVector<int> &U)
{
  int hh;
  
  hh=h-1;
  if(hh==0)hh=nstrats;
  while(U(j,hh)) {
    hh--;
    if(hh==0)hh=nstrats;
  }
  return hh;
}

int nfgSimpdiv::get_c(int j, int h, int nstrats,
		      const gbtPVector<int> &U)
{
  int hh;
  
  hh=get_b(j,h,nstrats,U);
  hh++;
  if(hh > nstrats)hh=1;
  return hh;
}

gbtRational nfgSimpdiv::getlabel(Gambit::MixedStrategyProfile<gbtRational> &yy,
			       gbtArray<int> &ylabel,
			       gbtPVector<gbtRational> &besty)
{
  int i,j,jj;
  gbtRational maxz,payoff,maxval;
  
  maxz=(gbtRational(-1000000));
  
  ylabel[1]=1;
  ylabel[2]=1;
  
  for(i=1;i<=yy.GetGame()->NumPlayers();i++) {
    payoff=gbtRational(0);
    maxval=(gbtRational(-1000000));
    jj=0;
    for(j=1;j<=yy.GetSupport().NumStrats(i);j++) {
      pay=yy.GetPayoff(i,i,j);
      payoff+=(yy(i,j)*pay);
      if(pay>maxval) {
	maxval=pay;
	jj=j;
      }
    }
    assert(jj>0);
    if((maxval-payoff)>maxz) {
      maxz=maxval-payoff;
      ylabel[1]=i;
      ylabel[2]=jj;
    }
  }
  if(maxz<bestz) {
    bestz=maxz;
    for(i=1;i<=yy.GetGame()->NumPlayers();i++)
      for(j=1;j<=yy.GetSupport().NumStrats(i);j++)
	besty(i,j)=yy(i,j);
  }
  return maxz;
}


//-------------------------------------------------------------------------
//               nfgSimpdiv: Main solution algorithm
//-------------------------------------------------------------------------

void PrintProfile(std::ostream &p_stream,
		  const std::string &p_label,
		  const Gambit::MixedStrategyProfile<gbtRational> &p_profile)
{
  p_stream << p_label;
  for (int i = 1; i <= p_profile.Length(); i++) {
    if (g_useFloat) {
      p_stream.setf(std::ios::fixed);
      p_stream << "," << std::setprecision(g_numDecimals) << ((double) p_profile[i]);
    }
    else {
      p_stream << "," << p_profile[i];
    }
  }

  p_stream << std::endl;
}

bool ReadProfile(std::istream &p_stream,
		 Gambit::MixedStrategyProfile<gbtRational> &p_profile)
{
  for (int i = 1; i <= p_profile.Length(); i++) {
    if (p_stream.eof() || p_stream.bad()) {
      return false;
    }

    p_stream >> p_profile[i];
    if (i < p_profile.Length()) {
      char comma;
      p_stream >> comma;
    }
  }

  // Read in the rest of the line and discard
  std::string foo;
  std::getline(p_stream, foo);
  return true;
}

gbtInteger find_lcd(const gbtVector<gbtRational> &vec)
{
  gbtInteger lcd(1);
  for (int i = vec.First(); i <= vec.Last(); i++) {
    lcd = lcm(vec[i].denominator(), lcd);
  }
  return lcd;
}

void nfgSimpdiv::Solve(const Gambit::Game &p_nfg, 
		       const Gambit::MixedStrategyProfile<gbtRational> &p_start)
{
  int qf,i,j,ii;

  // A raft of initializations moved here from the former constructor.
  // This algorithm is in need of some serious reorganization!
  t = 0;
  ibar = 1;

  if (m_leashLength == 0) {
    m_leashLength = 32000;  // not the best way to do this.  Change this!
  }
  bestz = 1.0e30;          // ditto

  gbtInteger k = find_lcd(p_start);
  d = gbtRational(1, k);
    
  Gambit::MixedStrategyProfile<gbtRational> y(p_start);
  if (g_verbose) {
    PrintProfile(std::cout, "start", y);
  }

  while (true) {
    const double TOL = 1.0e-10;
    d /= g_gridResize;
    maxz = Simplex(y);
    
    if (g_verbose) {
      PrintProfile(std::cout, ToText(d), y);
    }
    if (maxz < gbtRational(TOL)) break;
  }
    
  PrintProfile(std::cout, "NE", y);
}

void Randomize(Gambit::MixedStrategyProfile<gbtRational> &p_profile, int p_denom)
{
  Gambit::Game nfg = p_profile.GetGame();

  ((gbtVector<gbtRational> &) p_profile) = gbtRational(0);

  for (int pl = 1; pl <= nfg->NumPlayers(); pl++) {
    int sum = 0;
    for (int st = 1; sum < p_denom && st < nfg->GetPlayer(pl)->NumStrategies(); st++) {
      double r = ((double) rand() / ((double) (RAND_MAX) + 1.0));
      double x = r * (p_denom - sum + 1);
      int y = (int) x;
      // y is now uniform on [0, (p_denom-sum)].
      p_profile(pl, st) = gbtRational(y, p_denom);
      sum += y;
    }

    p_profile(pl, nfg->GetPlayer(pl)->NumStrategies()) = gbtRational(p_denom - sum, p_denom);
  }
}

void PrintBanner(std::ostream &p_stream)
{
  p_stream << "Compute Nash equilibria using simplicial subdivision\n";
  p_stream << "Gambit version " VERSION ", Copyright (C) 2005, The Gambit Project\n";
  p_stream << "This is free software, distributed under the GNU GPL\n\n";
}


void PrintHelp(char *progname)
{
  PrintBanner(std::cerr);
  std::cerr << "Usage: " << progname << " [OPTIONS]\n";
  std::cerr << "Accepts strategic game on standard input.\n";
  std::cerr << "With no options, computes one approximate Nash equilibrium.\n\n";

  std::cerr << "Options:\n";
  std::cerr << "  -d DECIMALS      show equilibria as floating point, with DECIMALS digits\n";
  std::cerr << "                   (default is to show as rational numbers)\n";
  std::cerr << "  -g MULT          granularity of grid refinement at each step (default is 2)\n";
  std::cerr << "  -h               print this help message\n";
  std::cerr << "  -r DENOM         generate random starting points with denominator DENOM\n";
  std::cerr << "  -n COUNT         number of starting points to generate (requires -r)\n";
  std::cerr << "  -s FILE          file containing starting points\n";
  std::cerr << "  -q               quiet mode (suppresses banner)\n";
  std::cerr << "  -v               verbose mode (shows intermediate output)\n";
  std::cerr << "                   (default is to only show equilibria)\n";
  exit(1);
}

int main(int argc, char *argv[])
{
  opterr = 0;
  std::string startFile;
  bool useRandom = false;
  int randDenom = 1, stopAfter = 1;
  bool quiet = false;

  int c;
  while ((c = getopt(argc, argv, "g:hvn:r:s:d:q")) != -1) {
    switch (c) {
    case 'd':
      g_numDecimals = atoi(optarg);
      g_useFloat = true;
      break;
    case 'g':
      g_gridResize = atoi(optarg);
      break;
    case 'h':
      PrintHelp(argv[0]);
      break;
    case 'r':
      useRandom = true;
      randDenom = atoi(optarg);
      break;
    case 'n':
      stopAfter = atoi(optarg);
      break;
    case 's':
      startFile = optarg;
      break;
    case 'q':
      quiet = true;
      break;
    case 'v':
      g_verbose = true;
      break;
    case '?':
      if (isprint(optopt)) {
	std::cerr << argv[0] << ": Unknown option `-" << ((char) optopt) << "'.\n";
      }
      else {
	std::cerr << argv[0] << ": Unknown option character `\\x" << optopt << "`.\n";
      }
      return 1;
    default:
      abort();
    }
  }

  if (!quiet) {
    PrintBanner(std::cerr);
  }



  Gambit::Game nfg;

  try {
    nfg = Gambit::ReadGame(std::cin);
  }
  catch (...) {
    return 1;
  }

  if (startFile != "") {
    std::ifstream startPoints(startFile.c_str());

    while (!startPoints.eof() && !startPoints.bad()) {
      Gambit::MixedStrategyProfile<gbtRational> start(nfg);
      if (ReadProfile(startPoints, start)) {
	nfgSimpdiv algorithm;
	algorithm.Solve(nfg, start);
      }
    }
  }
  else if (useRandom) {
    for (int i = 1; i <= stopAfter; i++) {
      Gambit::MixedStrategyProfile<gbtRational> start(nfg);
      Randomize(start, randDenom);

      nfgSimpdiv algorithm;
      algorithm.Solve(nfg, start);
    }
  }
  else {
    Gambit::MixedStrategyProfile<gbtRational> start(nfg);
  
    for (int pl = 1; pl <= nfg->NumPlayers(); pl++) {
      start(pl, 1) = gbtRational(1);
      for (int st = 2; st <= nfg->GetPlayer(pl)->NumStrategies(); st++) {
	start(pl, st) = gbtRational(0);
      }
    }

    nfgSimpdiv algorithm;
    algorithm.Solve(nfg, start);
  }
}