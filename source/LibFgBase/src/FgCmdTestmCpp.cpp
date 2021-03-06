//
// Copyright (c) 2015 Singular Inversions Inc. (facegen.com)
// Use, modification and distribution is subject to the MIT License,
// see accompanying file LICENSE.txt or facegen.com/base_library_license.txt
//
// Authors: Andrew Beatty
// Created: June 16, 2017
//
// Test C++ behaviour
//

#include "stdafx.h"

#include "FgCmd.hpp"
#include "FgSyntax.hpp"
#include "FgMetaFormat.hpp"
#include "FgImage.hpp"
#include "FgTestUtils.hpp"
#include "FgBuild.hpp"
#include "FgVersion.hpp"
#include "FgTime.hpp"

using namespace std;

static size_t rvoCount;

struct  FgTestmRvo
{
    FgDbls      data;
    FgTestmRvo() {}
    FgTestmRvo(double v) : data(1,v) {}
    FgTestmRvo(const FgTestmRvo & rhs) : data(rhs.data) {++rvoCount; }
};

FgTestmRvo
fgTestmRvo()
{
    FgDbls      t0(1,3.14159);
    return FgTestmRvo(t0[0]);
}

FgTestmRvo
fgTestmRvoMr(bool sel)
{
    FgDbls      t0(1,2.71828),
                t1(1,3.14159);
    if (sel)
        return FgTestmRvo(t0[0]);
    else
        return FgTestmRvo(t1[0]);
}

FgTestmRvo
fgTestmNrvo(bool sel)
{
    FgDbls      t0(1,2.71828),
                t1(1,3.14159);
    FgTestmRvo  ret;
    if (sel)
        ret = FgTestmRvo(t0[0]);
    else
        ret = FgTestmRvo(t1[0]);
    return ret;
}

FgTestmRvo
fgTestmNrvoMr(bool sel)
{
    FgDbls      t0(1,2.71828),
                t1(1,3.14159);
    FgTestmRvo  ret;
    if (sel) {
        ret = FgTestmRvo(t0[0]);
        return ret;
    }
    else {
        ret = FgTestmRvo(t1[0]);
        return ret;
    }
}

static
void
rvo(const FgArgs &)
{
    FgTestmRvo      t;
    rvoCount = 0;
    t = fgTestmRvo();
    fgout << fgnl << "RVO copies: " << rvoCount;
    rvoCount = 0;
    t = fgTestmRvoMr(true);
    t = fgTestmRvoMr(false);
    fgout << fgnl << "RVO with multiple returns (both paths) copies: " << rvoCount;
    rvoCount = 0;
    t = fgTestmNrvo(true);
    t = fgTestmNrvo(false);
    fgout << fgnl << "NRVO copies (both assignment paths): " << rvoCount;
    rvoCount = 0;
    t = fgTestmNrvoMr(true);
    t = fgTestmNrvoMr(false);
    fgout << fgnl << "NRVO with multiple returns (both paths) copies: " << rvoCount;
}

static
void
speedExp(const FgArgs &)
{
    double      val = 0,
                inc = 2.718281828,
                mod = 3.141592653,
                acc = 0;
    size_t      reps = 10000000;
    FgTimer     tm;
    for (size_t ii=0; ii<reps; ++ii) {
        acc += exp(-val);
        val += inc;
        if (val > mod)
            val -= mod;
    }
    fgout << fgnl << "exp() time: " << 1000000.0 * tm.readMs() / reps << " ns  (dummy val: " << acc << ")";
}

static
void
fgexp(const FgArgs &)
{
    double      maxRel = 0,
                totRel = 0;
    size_t      cnt = 0;
    for (double dd=0; dd<5; dd+=0.001) {
        double  baseline = exp(-dd),
                test = fgExpFast(-dd),
                meanVal = (test+baseline) * 0.5,
                relDel = (test-baseline) / meanVal;
        maxRel = fgMax(maxRel,relDel);
        totRel += relDel;
        ++cnt;
    }
    fgout << "Max Rel Del: " << maxRel << " mean rel del: " << totRel / cnt;
    double      val = 0,
                inc = 2.718281828,
                mod = 3.141592653,
                acc = 0;
    size_t      reps = 10000000;
    FgTimer     tm;
    for (size_t ii=0; ii<reps; ++ii) {
        acc += fgExpFast(-val);
        val += inc;
        if (val > mod)
            val -= mod;
    }
    fgout << fgnl << "exp() time: " << 1000000.0 * tm.readMs() / reps << " ns  (dummy val: " << acc << ")";
}

static
void
any(const FgArgs &)
{
    boost::any      v0 = 42,
                    v1 = v0;
    int *           v0Ptr = boost::any_cast<int>(&v0);
    *v0Ptr = 7;
    fgout << fgnl << "Original small value: " << *v0Ptr << " but copy remains at " << boost::any_cast<int>(v1);
    // Now try with a heavy object that is not subject to small value optimization (16 bytes) onto the stack:
    v0 = FgMat44D(42);
    v1 = v0;
    FgMat44D *      v0_ptr = boost::any_cast<FgMat44D>(&v0);
    (*v0_ptr)[0] = 7;
    fgout << fgnl << "Original big value: " << (*v0_ptr)[0] << " but copy remains at " << boost::any_cast<FgMat44D>(v1)[0];
}

void
fgCmdTestmCpp(const FgArgs & args)
{
    vector<FgCmd>   cmds;
    cmds.push_back(FgCmd(rvo,"rvo","Return value optimization / copy elision"));
    cmds.push_back(FgCmd(speedExp,"exp","Measure the speed of library exp(double)"));
    cmds.push_back(FgCmd(fgexp,"fgexp","Test and mesaure speed of interal optimized exp"));
    cmds.push_back(FgCmd(any,"any","Test boost any copy semantics"));
    fgMenu(args,cmds);
}

// */
