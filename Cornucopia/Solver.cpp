/*--
    Solver.cpp  

    This file is part of the Cornucopia curve sketching library.
    Copyright (C) 2010 Ilya Baran (ibaran@mit.edu)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "Solver.h"
#include <Eigen/Cholesky>
#include <iostream> //TMP

using namespace std;
using namespace Eigen;
NAMESPACE_Cornu

LSSolver::LSSolver(LSProblem *problem, const vector<LSBoxConstraint> &constraints)
: _problem(problem), _constraints(constraints), _damping(1.), _maxIter(100)
{
};

VectorXd LSSolver::solve(const VectorXd &guess)
{
    VectorXd best;
    double bestError = 1e100;
    VectorXd x = guess;
    LSEvalData *evalData = _problem->createEvalData();

    set<LSBoxConstraint> activeSet = _clamp(x);

    for(int iter = 0; iter < _maxIter; ++iter)
    {
        _problem->eval(x, evalData);
        VectorXd delta;

        double error = evalData->error();
        //printf("Iter = %d, error = %lf\n", iter, error);
        if(error < bestError)
        {
            if(error < 1e-8)
                return x;
            bestError = error;
            best = x;
        }

        evalData->solveForDelta(_damping, delta, activeSet);

        int newConstraint = _project(x, delta);

        if(newConstraint != -1)
            activeSet.insert(_constraints[newConstraint]);

        x += delta;
    }

    double error = _problem->error(x, evalData);
    //printf("Final error = %lf\n", error);
    if(error < bestError)
    {
        best = x;
    }

    delete evalData;
    return best;
}

set<LSBoxConstraint> LSSolver::_clamp(VectorXd &x)
{
    set<LSBoxConstraint> out;
    for(int i = 0; i < (int)_constraints.size(); ++i)
    {
        const LSBoxConstraint &c = _constraints[i];
        if((x[c.index] - c.value) * c.sign < 0.)
        {
            x[c.index] = c.value;
            out.insert(c);
            //cout << "Clamping constraint " << i << endl;
        }
    }
    return out;
}

int LSSolver::_project(const VectorXd &from, VectorXd &delta)
{
    int closestConstraint = -1;
    double minScale = 1.;

    for(int i = 0; i < (int)_constraints.size(); ++i)
    {
        const LSBoxConstraint &c = _constraints[i];
        if(fabs(delta[c.index]) < 1e-16)
            continue; //not enough of a change to project

        double scale = (c.value - from[c.index]) / delta[c.index];

        if((from[c.index] + delta[c.index] - c.value) * c.sign >= 0.)
            continue;

        if(scale < minScale)
        {
            minScale = scale;
            closestConstraint = i;
        }
    }

    if(closestConstraint >= 0)
    {
        delta *= minScale;
        //cout << "Projecting up to constraint " << closestConstraint << " by " << minScale << endl;
    }

    return closestConstraint;
}

bool LSSolver::verifyDerivatives(const Eigen::VectorXd &pt) const
{
    LSEvalData *evalData = _problem->createEvalData();
    _problem->eval(pt, evalData);

    MatrixXd exactDer = evalData->errVecDer();
    MatrixXd numDer = exactDer;

    double eps = 1e-6;
    for(int i = 0; i < numDer.cols(); ++i)
    {
        VectorXd mod = pt;
        mod[i] += eps;

        _problem->eval(mod, evalData);
        VectorXd plus = evalData->errVec();

        mod[i] = pt[i] - eps;
        _problem->eval(mod, evalData);
        VectorXd minus = evalData->errVec();

        numDer.col(i) = (plus - minus) / (2 * eps);
    }

    double err = (numDer - exactDer).norm();

    //TODO: just print the error for now
    Debugging::get()->printf("Derivative Error = %lf", err);

    for(int i = 0; i < numDer.cols(); ++i)
        Debugging::get()->printf("Col %d err = %lf", i, (numDer.col(i) - exactDer.col(i)).norm());
    for(int i = 0; i < numDer.rows(); ++i)
        Debugging::get()->printf("Row %d err = %lf", i, (numDer.row(i) - exactDer.row(i)).norm());

    delete evalData;

    return true;
}

void LSDenseEvalData::solveForDelta(double damping, VectorXd &out, set<LSBoxConstraint> &constraints)
{
    int vars = (int)_errDer.cols();
    if(constraints.empty())
    {
        LDLT<MatrixXd> ldlt(MatrixXd::Identity(vars, vars) * damping + _errDer.transpose() * _errDer);
        out = ldlt.solve(-_errDer.transpose() * _err);
    }
    else
    {
        VectorXd rhs = -_err;

        vector<bool> constraintIndices(vars, false);
        for(set<LSBoxConstraint>::const_iterator it = constraints.begin(); it != constraints.end(); ++it)
            constraintIndices[it->index] = true;

        if(vars > (int)constraints.size())
        {
            MatrixXd lhs(_errDer.rows(), vars - (int)constraints.size());
            int offs = 0;
            for(int i = 0; i < vars; ++i)
            {
                if(constraintIndices[i])
                {
                    ++offs;
                    continue;
                }
                lhs.col(i - offs) = _errDer.col(i);
            }

            LDLT<MatrixXd> ldlt(MatrixXd::Identity(lhs.cols(), lhs.cols()) * damping + lhs.transpose() * lhs);
            VectorXd x = ldlt.solve(lhs.transpose() * rhs);
            
            out.resize(_errDer.cols());
            offs = 0;

            for(int i = 0; i < vars; ++i)
            {
                if(constraintIndices[i])
                {
                    out[i] = 0.;
                    ++offs;
                    continue;
                }
                out[i] = x[i - offs];
            }
        }
        else //as many variables as constraints
        {
            out = VectorXd::Zero(_errDer.cols());
        }

        //check which constraints we don't need
        VectorXd gradient = _errDer.transpose() * (_errDer * out - rhs);
        for(set<LSBoxConstraint>::iterator it = constraints.begin(); it != constraints.end(); )
        {
            set<LSBoxConstraint>::iterator next = it;
            ++next;
            if(gradient[it->index] * it->sign < 0)
            {
                //cout << "Unsetting constraint on variable at index " << it->index << endl;
                constraints.erase(it);
            }
            it = next;
        }
    }
}


END_NAMESPACE_Cornu


