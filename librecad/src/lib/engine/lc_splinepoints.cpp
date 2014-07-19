/****************************************************************************
**
** This file is part of the LibreCAD project, a 2D CAD program
**
** Copyright (C) 2010 R. van Twisk (librecad@rvt.dds.nl)
** Copyright (C) 2014 Dongxu Li (dongxuli2011@gmail.com)
** Copyright (C) 2014 Pevel Krejcir (pavel@pamsoft.cz)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**********************************************************************/


#include "lc_splinepoints.h"

#include "rs_debug.h"
#include "rs_graphicview.h"
#include "rs_painter.h"
#include "rs_graphic.h"
#include "rs_painterqt.h"
#include "lc_quadratic.h"

RS_Vector GetQuadPoint(const RS_Vector& x1,
	const RS_Vector& c1, const RS_Vector& x2, double dt)
{
	return x1*(1.0 - dt)*(1.0 - dt) + c1*2.0*dt*(1.0 - dt) + x2*dt*dt;
}

RS_Vector GetQuadDir(const RS_Vector& x1,
	const RS_Vector& c1, const RS_Vector& x2, double dt)
{
	RS_Vector vStart = c1 - x1;
	RS_Vector vEnd = x2 - c1;
	RS_Vector vRes(false);

	double dDist = (vEnd - vStart).squared();
	if(dDist > RS_TOLERANCE2)
	{
		vRes = vStart*(1.0 - dt) + vEnd*dt;
		dDist = vRes.magnitude();
		if(dDist < RS_TOLERANCE) return RS_Vector(false);

		return vRes/dDist;
	}

	dDist = (x2 - x1).magnitude();
	if(dDist > RS_TOLERANCE)
	{
		return (x2 - x1)/dDist;
	}

	return vRes;
}

RS_Vector GetSubQuadControlPoint(const RS_Vector& x1,
	const RS_Vector& c1, const RS_Vector& x2, double dt1, double dt2)
{
	return x1*(1.0 - dt1)*(1.0 - dt2) + c1*dt1*(1.0 - dt2) +
		c1*dt2*(1.0 - dt1) + x2*dt1*dt2;
}

double LenInt(double x)
{
	double y = sqrt(1 + x*x); 
	return log(x + y) + x*y;
}

double GetQuadLength(const RS_Vector& x1, const RS_Vector& c1, const RS_Vector& x2,
	double t1, double t2)
{
	RS_Vector xh1 = (c1 - x1)*2.0;
	RS_Vector xh2 = (x2 - c1)*2.0;
	RS_Vector xd1 = xh2 - xh1;
	RS_Vector xd2 = xh1;

	double dx1 = xd1.squared();
	double dx2 = xd2.squared();
	double dx12 = xd1.x*xd2.x + xd1.y*xd2.y;
	double dDet = dx1*dx2 - dx12*dx12; // always >= 0 from Schwarz inequality

	double dRes = 0.0;

	if(dDet > RS_TOLERANCE)
	{
		double dA = sqrt(dDet);
		double v1 = (dx1*t1 + dx12)/dA;
		double v2 = (dx1*t2 + dx12)/dA;
		dRes = (LenInt(v2) - LenInt(v1))*dDet/2.0/dx1/sqrt(dx1);
	}
	else
	{
		if(dx1 < RS_TOLERANCE)
		{
			dRes = sqrt(dx2)*(t2 - t1);
		}
		else
		{
			dx2 = sqrt(dx1);
			dRes = (t2 - t1)*(dx2*(t2 + t1)/2.0 + dx12/dx2);
		}
	}

	return(dRes);
}

double GetQuadLengthDeriv(const RS_Vector& x1, const RS_Vector& c1, const RS_Vector& x2,
	double t2)
{
	RS_Vector xh1 = (c1 - x1)*2.0;
	RS_Vector xh2 = (x2 - c1)*2.0;
	RS_Vector xd1 = xh2 - xh1;
	RS_Vector xd2 = xh1;

	double dx1 = xd1.squared();
	double dx2 = xd2.squared();
	double dx12 = xd1.x*xd2.x + xd1.y*xd2.y;
	double dDet = dx1*dx2 - dx12*dx12; // always >= 0 from Schwarz inequality

	double dRes = 0.0;

	if(dDet > RS_TOLERANCE)
	{
		double dA = sqrt(dDet);
		double v2 = (dx1*t2 + dx12)/dA;
		double v3 = v2*v2;
		double v4 = 1.0 + v3;
		double v5 = sqrt(v4);
		dRes = ((v2 + v5)/(v4 + v2*v5) + (2.0*v3 + 1.0)/v5)*dA/2.0/sqrt(dx1);
	}
	else
	{
		if(dx1 < RS_TOLERANCE) dRes = sqrt(dx2);
		else
		{
			dx2 = sqrt(dx1);
			dRes = dx2*t2 + dx12/dx2;
		}
	}

	return(dRes);
}

double GetQuadPointAtDist(const RS_Vector& x1, const RS_Vector& c1, const RS_Vector& x2,
	double t1, double dDist)
{
	RS_Vector xh1 = (c1 - x1)*2.0;
	RS_Vector xh2 = (x2 - c1)*2.0;
	RS_Vector xd1 = xh2 - xh1;
	RS_Vector xd2 = xh1;

	double dx1 = xd1.squared();
	double dx2 = xd2.squared();
	double dx12 = xd1.x*xd2.x + xd1.y*xd2.y;
	double dDet = dx1*dx2 - dx12*dx12; // always >= 0 from Schwarz inequality

	double dRes = RS_MAXDOUBLE;
    double a0, a1, a2/*, a3, a4*/;

	std::vector<double> dCoefs(0, 0.);
	std::vector<double> dSol(0, 0.);

	if(dDet > RS_TOLERANCE)
	{
		double dA = sqrt(dDet);
		double v1 = (dx1*t1 + dx12)/dA;
		//double v2 = (dx1*t2 + dx12)/dA;
		//dDist = (LenInt(v2) - LenInt(v1))*dDet/2.0/dx1/sqrt(dx1);
		//LenInt(v2) = 2.0*dx1*sqrt(dx1)*dDist/dDet + LenInt(v1);
		double dB = 2.0*dx1*sqrt(dx1)*dDist/dDet + LenInt(v1);

		dCoefs.push_back(0.0);
		dCoefs.push_back(0.0);
		dCoefs.push_back(2.0*dB);
		dCoefs.push_back(-dB*dB);
		dSol = RS_Math::quarticSolver(dCoefs);

		dRes = t1;
		a1 = 0;
        for(const double& d: dSol)
		{
            a0 = (d*dA - dx12)/dx1;
			a2 = GetQuadLength(x1, c1, x2, t1, a0);
			if(fabs(dDist - a2) < fabs(dDist - a1))
			{
				a1 = a2;
				dRes = a0;
			}
		}

		// we believe we are pretty close to the solution at the moment
		// so we only perform three iterations
		for(int i = 0; i < 3; i++)
		{
			a1 = GetQuadLength(x1, c1, x2, t1, dRes) - dDist;
			a2 = GetQuadLengthDeriv(x1, c1, x2, dRes);
			if(fabs(a2) > RS_TOLERANCE)
			{
				dRes -= a1/a2;
			}
		}
	}
	else
	{
		if(dx1 < RS_TOLERANCE)
		{
			if(dx2 > RS_TOLERANCE) dRes = t1 + dDist/sqrt(dx2);
		}
		else
		{
			dx2 = sqrt(dx1);
			//dRes = (t2 - t1)*(dx2*(t2 + t1)/2.0 + dx12/dx2);

			a0 = dx2/2.0;
			a1 = dx12/dx2;
			a2 = -dDist - a0*t1*t1 - a1*t1;

			dCoefs.push_back(a1/a0);
			dCoefs.push_back(a2/a0);
			dSol = RS_Math::quadraticSolver(dCoefs);

			if(dSol.size() > 0)
			{
				dRes = dSol[0];
				if(dSol.size() > 1)
				{
					a1 = GetQuadLength(x1, c1, x2, t1, dRes);
					a2 = GetQuadLength(x1, c1, x2, t1, dSol[1]);
					if(fabs(dDist - a2) < fabs(dDist - a1))
						dRes = dSol[1];
				}
			}
		}
	}

	return(dRes);
}

RS_Vector GetThreePointsControl(const RS_Vector& x1, const RS_Vector& x2, const RS_Vector& x3)
{
	double dl1 = (x2 - x1).magnitude();
	double dl2 = (x3 - x2).magnitude();
	double dt = dl1/(dl1 + dl2);

	if(dt < RS_TOLERANCE || dt > 1.0 - RS_TOLERANCE) return RS_Vector(false);

	RS_Vector vRes = (x2 - x1*(1.0 - dt)*(1.0 - dt) - x3*dt*dt)/dt/(1 - dt)/2.0;
	return vRes;
}


// RS_SplinePoints

/**
 * Constructor.
 */
LC_SplinePoints::LC_SplinePoints(RS_EntityContainer* parent,
    const LC_SplinePointsData& d) : RS_AtomicEntity(parent), data(d)
{
	calculateBorders();
}

/**
 * Destructor.
 */
LC_SplinePoints::~LC_SplinePoints()
{
}

RS_Entity* LC_SplinePoints::clone()
{
    LC_SplinePoints* l = new LC_SplinePoints(*this);
	l->initId();
	return l;
}

void LC_SplinePoints::UpdateQuadExtent(const RS_Vector& x1, const RS_Vector& c1, const RS_Vector& x2)
{
    RS_Vector locMinV = RS_Vector::minimum(x1, x2);
    RS_Vector locMaxV = RS_Vector::maximum(x1, x2);

	RS_Vector vDer = x2 - c1*2.0 + x1;
	double dt, dx;

	if(fabs(vDer.x) > RS_TOLERANCE)
	{
		dt = (x1.x - c1.x)/vDer.x;
		if(dt > RS_TOLERANCE && dt < 1.0 - RS_TOLERANCE)
		{
			dx = x1.x*(1.0 - dt)*(1.0 - dt) + 2.0*c1.x*dt*(1.0 - dt) + x2.x*dt*dt;
			if(dx < locMinV.x) locMinV.x = dx;
			if(dx > locMaxV.x) locMaxV.x = dx;
		}
	}

	if(fabs(vDer.y) > RS_TOLERANCE)
	{
		dt = (x1.y - c1.y)/vDer.y;
		if(dt > RS_TOLERANCE && dt < 1.0 - RS_TOLERANCE)
		{
			dx = x1.y*(1.0 - dt)*(1.0 - dt) + 2.0*c1.y*dt*(1.0 - dt) + x2.y*dt*dt;
			if(dx < locMinV.y) locMinV.y = dx;
			if(dx > locMaxV.y) locMaxV.y = dx;
		}
	}

    minV = RS_Vector::minimum(locMinV, minV);
    maxV = RS_Vector::maximum(locMaxV, maxV);
}

void LC_SplinePoints::calculateBorders()
{
	minV = RS_Vector(false);
	maxV = RS_Vector(false);

	int iPoints = data.splinePoints.count();

	if(iPoints < 1) return;

	minV = data.splinePoints.at(0);
	maxV = minV;

	if(iPoints < 2) return;

	RS_Vector vStart(false), vControl(false), vEnd(false), vRes(false);

	if(iPoints < 3)
	{
		vEnd = data.splinePoints.at(1);
		minV = RS_Vector::minimum(vEnd, minV);
		maxV = RS_Vector::maximum(vEnd, maxV);
		return;
	}

	int n = data.controlPoints.count();
//	if(n < 1) return;

	if(data.closed)
	{
		vStart = (data.controlPoints.at(n - 1) + data.controlPoints.at(0))/2.0;
		vControl = data.controlPoints.at(0);
		vEnd = (data.controlPoints.at(0) + data.controlPoints.at(1))/2.0;
		UpdateQuadExtent(vStart, vControl, vEnd);

		for(int i = 1; i < n - 1; i++)
		{
			vStart = vEnd;
			vControl = data.controlPoints.at(i);
			vEnd = (data.controlPoints.at(i) + data.controlPoints.at(i + 1))/2.0;
			UpdateQuadExtent(vStart, vControl, vEnd);
		}

		vStart = vEnd;
		vControl = data.controlPoints.at(n - 1);
		vEnd = (data.controlPoints.at(n - 1) + data.controlPoints.at(0))/2.0;
		UpdateQuadExtent(vStart, vControl, vEnd);
	}
	else
	{
		if(iPoints == 3)
		{
			vStart = data.splinePoints.at(0);
			vEnd = data.splinePoints.at(2);
			vControl = GetThreePointsControl(vStart, data.splinePoints.at(1), vEnd);

			if(vControl.valid)
			{
				UpdateQuadExtent(vStart, vControl, vEnd);
			}
			else
			{
				minV = RS_Vector::minimum(vEnd, minV);
				maxV = RS_Vector::maximum(vEnd, maxV);
			}

			return;
		}

		vStart = data.splinePoints.at(0);
		vControl = data.controlPoints.at(0);
		vEnd = (data.controlPoints.at(0) + data.controlPoints.at(1))/2.0;
		UpdateQuadExtent(vStart, vControl, vEnd);

		for(int i = 1; i < n - 1; i++)
		{
			vStart = vEnd;
			vControl = data.controlPoints.at(i);
			vEnd = (data.controlPoints.at(i) + data.controlPoints.at(i + 1))/2.0;
			UpdateQuadExtent(vStart, vControl, vEnd);
		}

		vStart = vEnd;
		vControl = data.controlPoints.at(n - 1);
		vEnd = data.splinePoints.at(iPoints - 1);
		UpdateQuadExtent(vStart, vControl, vEnd);
	}
	return;
}

RS_VectorSolutions LC_SplinePoints::getRefPoints()
{
	RS_VectorSolutions ret(data.splinePoints.count());

	for(int i = 0; i < data.splinePoints.count(); i++)
	{
		ret.set(i, data.splinePoints.at(i));
	}

	return ret;
}

RS_Vector LC_SplinePoints::getNearestRef(const RS_Vector& coord,
	double* dist)
{
    return RS_Entity::getNearestRef(coord, dist);
}

RS_Vector LC_SplinePoints::getNearestSelectedRef(const RS_Vector& coord,
	double* dist)
{
    return RS_Entity::getNearestSelectedRef(coord, dist);
}

/** @return Start point of the entity */
RS_Vector LC_SplinePoints::getStartPoint() const
{
	if(data.closed) return RS_Vector(false);

	int iCount = data.splinePoints.count();
	if(iCount < 1) return RS_Vector(false);

    return data.splinePoints.at(0);
}

/** @return End point of the entity */
RS_Vector LC_SplinePoints::getEndPoint() const
{
	if(data.closed) return RS_Vector(false);

	int iCount = data.splinePoints.count();
	if(iCount < 1) return RS_Vector(false);

    return data.splinePoints.at(iCount - 1);
}

RS_Vector LC_SplinePoints::getNearestEndpoint(const RS_Vector& coord,
	double* dist) const
{
	double minDist = RS_MAXDOUBLE;
	RS_Vector ret(false);
	if(!data.closed) // no endpoint for closed spline
	{
		RS_Vector vp1(getStartPoint());
		RS_Vector vp2(getEndPoint());
		double d1 = (coord-vp1).squared();
		double d2 = (coord-vp2).squared();
		if(d1 < d2)
		{
			ret = vp1;
			minDist = sqrt(d1);
		}
		else
		{
			ret=vp2;
			minDist = sqrt(d2);
		}
	}
	if(dist)
	{
		*dist = minDist;
	}
	return ret;
}

double GetDistToLine(const RS_Vector& coord, const RS_Vector& x1,
	const RS_Vector& x2, double* dist)
{
	double ddet = (x2 - x1).squared();
	if(ddet < RS_TOLERANCE)
	{
		*dist = (coord - x1).magnitude();
		return 0.0;
	}

	double dt = ((coord.x - x1.x)*(x2.x - x1.x) + (coord.y - x1.y)*(x2.y - x1.y))/ddet;

	if(dt < RS_TOLERANCE)
	{
		*dist = (coord - x1).magnitude();
		return 0.0;
	}

	if(dt > 1.0 - RS_TOLERANCE)
	{
		*dist = (coord - x2).magnitude();
		return 1.0;
	}

	RS_Vector vRes = x1*(1.0 - dt) + x2*dt;
	*dist = (coord - vRes).magnitude();
	return dt;
}

RS_Vector GetQuadAtPoint(const RS_Vector& x1, const RS_Vector& c1,
	const RS_Vector& x2, double dt)
{
	RS_Vector vRes = x1*(1.0 - dt)*(1.0 - dt) +
		c1*2.0*dt*(1.0 - dt) + x2*dt*dt;
	return vRes;
}

RS_Vector GetQuadDirAtPoint(const RS_Vector& x1, const RS_Vector& c1,
	const RS_Vector& x2, double dt)
{
	RS_Vector vRes = (c1 - x1)*(1.0 - dt) + (x2 - c1)*dt;
	return vRes;
}

double GetDistToQuadAtPointSquared(const RS_Vector& coord, const RS_Vector& x1,
	const RS_Vector& c1, const RS_Vector& x2, double dt)
{
	if(dt < RS_TOLERANCE)
	{
		return (coord - x1).squared();
	}

	if(dt > 1.0 - RS_TOLERANCE)
	{
		return (coord - x2).squared();
	}

	RS_Vector vx = GetQuadAtPoint(x1, c1, x2, dt);
	return (coord - vx).squared();
}

// returns true if the new distance was smaller than previous one
bool SetNewDist(bool bResSet, double dNewDist, double dNewT,
	double *pdDist, double *pdt)
{
	bool bRes = false;
	if(bResSet)
	{
		if(dNewDist < *pdDist)
		{
			*pdDist = dNewDist;
			*pdt = dNewT;
			bRes = true;
		}
	}
	else
	{
		*pdDist = dNewDist;
		*pdt = dNewT;
		bRes = true;
	}
	return bRes;
}

double GetDistToQuadSquared(const RS_Vector& coord, const RS_Vector& x1,
	const RS_Vector& c1, const RS_Vector& x2, double* dist)
{
	double a1, a2, a3, a4;
	a1 = (x2.x - 2.0*c1.x + x1.x)*(x2.x - 2.0*c1.x + x1.x) +
		(x2.y - 2.0*c1.y + x1.y)*(x2.y - 2.0*c1.y + x1.y);
	a2 = 3.0*((c1.x - x1.x)*(x2.x - 2.0*c1.x + x1.x) +
		(c1.y - x1.y)*(x2.y - 2.0*c1.y + x1.y));
	a3 = 2.0*(c1.x - x1.x)*(c1.x - x1.x) +
		(x1.x - coord.x)*(x2.x - 2.0*c1.x + x1.x) +
		2.0*(c1.y - x1.y)*(c1.y - x1.y) +
		(x1.y - coord.y)*(x2.y - 2.0*c1.y + x1.y);
	a4 = (x1.x - coord.x)*(c1.x - x1.x) + (x1.y - coord.y)*(c1.y - x1.y);

	std::vector<double> dCoefs(0, 0.);
	std::vector<double> dSol(0, 0.);

	if(fabs(a1) > RS_TOLERANCE) // solve as cubic
	{
		dCoefs.push_back(a2/a1);
		dCoefs.push_back(a3/a1);
		dCoefs.push_back(a4/a1);
		dSol = RS_Math::cubicSolver(dCoefs);
	}
	else if(fabs(a2) > RS_TOLERANCE) // solve as quadratic
	{
		dCoefs.push_back(a3/a2);
		dCoefs.push_back(a4/a2);
		dSol = RS_Math::quadraticSolver(dCoefs);
	}
	else if(fabs(a3) > RS_TOLERANCE) // solve as linear
	{
		dSol.push_back(-a4/a3);
	}
	else return -1.0;

	bool bResSet = false;
	double dDist, dNewDist;
	double dRes;
	for(int i = 0; i < dSol.size(); i++)
	{
		dNewDist = GetDistToQuadAtPointSquared(coord, x1, c1, x2, dSol[i]);
		SetNewDist(bResSet, dNewDist, dSol[i], &dDist, &dRes);
		bResSet = true;
	}

	dNewDist = (coord - x1).squared();
	SetNewDist(bResSet, dNewDist, 0.0, &dDist, &dRes);

	dNewDist = (coord - x2).squared();
	SetNewDist(bResSet, dNewDist, 1.0, &dDist, &dRes);

	*dist = dDist;

	return dRes;
}

// returns true if pvControl is set
bool LC_SplinePoints::GetQuadPoints(int iSeg, RS_Vector *pvStart, RS_Vector *pvControl,
	RS_Vector *pvEnd) const
{
	int iPoints = data.splinePoints.count();

	if(iPoints < 3)
	{
		*pvStart = data.splinePoints.at(0);
		*pvEnd = data.splinePoints.at(1);
		return false;
	}

	int n = data.controlPoints.count();

	int i1 = iSeg - 1;
	int i2 = iSeg; 
	int i3 = iSeg + 1;

	if(data.closed)
	{
		if(i1 < 0) i1 = n - 1;
		if(i3 > n - 1) i3 = 0;

		*pvStart = (data.controlPoints.at(i1) + data.controlPoints.at(i2))/2.0;
		*pvControl = data.controlPoints.at(i2);
		*pvEnd = (data.controlPoints.at(i2) + data.controlPoints.at(i3))/2.0;
	}
	else
	{
		if(iPoints == 3)
		{
			*pvStart = data.splinePoints.at(0);
			*pvEnd = data.splinePoints.at(2);
			*pvControl = GetThreePointsControl(*pvStart, data.splinePoints.at(1), *pvEnd);

			if(pvControl->valid) return true;

			return false;
		}

		if(i1 < 0) *pvStart = data.splinePoints.at(0);
		else *pvStart = (data.controlPoints.at(i1) + data.controlPoints.at(i2))/2.0;
		*pvControl = data.controlPoints.at(i2);
		if(i3 > n - 1) *pvEnd = data.splinePoints.at(iPoints - 1);
		else *pvEnd = (data.controlPoints.at(i2) + data.controlPoints.at(i3))/2.0;
	}
	return true;
}

// returns the index to the nearest segment, dt holds the t parameter
int LC_SplinePoints::GetNearestQuad(const RS_Vector& coord,
	double* dist, double* dt) const
{
	int iPoints = data.splinePoints.count();

	RS_Vector vStart(false), vControl(false), vEnd(false), vRes(false);

	if(iPoints < 1) return -1;
	if(iPoints < 2)
	{
		vStart = data.splinePoints.at(0);
		if(dist) *dist = (coord - vStart).magnitude();
		return 0;
	}

	double dDist, dNewDist;

	if(iPoints < 3)
	{
		vStart = data.splinePoints.at(0);
		vEnd = data.splinePoints.at(1);
		*dt = GetDistToLine(coord, vStart, vEnd, &dDist);
		if(dist) *dist = dDist;
		return 0;
	}

	int n = data.controlPoints.count();

	double dRes, dNewRes;
	int iRes = -1;

	if(data.closed)
	{
		vStart = (data.controlPoints.at(n - 1) + data.controlPoints.at(0))/2.0;
		vControl = data.controlPoints.at(0);
		vEnd = (data.controlPoints.at(0) + data.controlPoints.at(1))/2.0;

		dRes = GetDistToQuadSquared(coord, vStart, vControl, vEnd, &dDist);
		iRes = 0;

		for(int i = 1; i < n - 1; i++)
		{
			vStart = vEnd;
			vControl = data.controlPoints.at(i);
			vEnd = (data.controlPoints.at(i) + data.controlPoints.at(i + 1))/2.0;

			dNewRes = GetDistToQuadSquared(coord, vStart, vControl, vEnd, &dNewDist);
			if(SetNewDist(true, dNewDist, dNewRes, &dDist, &dRes)) iRes = i;
		}

		vStart = vEnd;
		vControl = data.controlPoints.at(n - 1);
		vEnd = (data.controlPoints.at(n - 1) + data.controlPoints.at(0))/2.0;

		dNewRes = GetDistToQuadSquared(coord, vStart, vControl, vEnd, &dNewDist);
		if(SetNewDist(true, dNewDist, dNewRes, &dDist, &dRes)) iRes = n - 1;
	}
	else
	{
		if(iPoints == 3)
		{
			vStart = data.splinePoints.at(0);
			vEnd = data.splinePoints.at(2);
			vControl = GetThreePointsControl(vStart, data.splinePoints.at(1), vEnd);

			if(vControl.valid)
			{
				dRes = GetDistToQuadSquared(coord, vStart, vControl, vEnd, &dDist);
			}
			else
			{
				dRes = GetDistToLine(coord, vStart, vEnd, &dDist);
			}
			*dt = dRes;
			if(dist) *dist = sqrt(dDist);
			return 0;
		}

		vStart = data.splinePoints.at(0);
		vControl = data.controlPoints.at(0);
		vEnd = (data.controlPoints.at(0) + data.controlPoints.at(1))/2.0;

		dRes = GetDistToQuadSquared(coord, vStart, vControl, vEnd, &dDist);
		iRes = 0;

		for(int i = 1; i < n - 1; i++)
		{
			vStart = vEnd;
			vControl = data.controlPoints.at(i);
			vEnd = (data.controlPoints.at(i) + data.controlPoints.at(i + 1))/2.0;

			dNewRes = GetDistToQuadSquared(coord, vStart, vControl, vEnd, &dNewDist);
			if(SetNewDist(true, dNewDist, dNewRes, &dDist, &dRes)) iRes = i;
		}

		vStart = vEnd;
		vControl = data.controlPoints.at(n - 1);
		vEnd = data.splinePoints.at(iPoints - 1);

		dNewRes = GetDistToQuadSquared(coord, vStart, vControl, vEnd, &dNewDist);
		if(SetNewDist(true, dNewDist, dNewRes, &dDist, &dRes)) iRes = n - 1;
	}

	*dt = dRes;
	if(dist) *dist = sqrt(dDist);
	return iRes;
}

RS_Vector LC_SplinePoints::getNearestPointOnEntity(const RS_Vector& coord,
	bool onEntity, double* dist, RS_Entity** entity) const
{
	int iPoints = data.splinePoints.count();

	RS_Vector vStart(false), vControl(false), vEnd(false), vRes(false);

	if(iPoints < 1) return vStart;
	if(iPoints < 2)
	{
		vStart = data.splinePoints.at(0);
		if(dist) *dist = (coord - vStart).magnitude();
		if(entity) *entity = const_cast<LC_SplinePoints*>(this);
		return vStart;
	}

	double dt = 0.0;
	int iQuad = GetNearestQuad(coord, dist, &dt);
	if(GetQuadPoints(iQuad, &vStart, &vControl, &vEnd))
		vRes = GetQuadAtPoint(vStart, vControl, vEnd, dt);
	else vRes = vStart*(1.0 - dt) + vEnd*dt;

	if(entity) *entity = const_cast<LC_SplinePoints*>(this);
	return vRes;
}

double LC_SplinePoints::getDistanceToPoint(const RS_Vector& coord,
	RS_Entity** entity, RS2::ResolveLevel level, double solidDist) const
{
	double dDist = RS_MAXDOUBLE;
	getNearestPointOnEntity(coord, true, &dDist, entity);
	return dDist;
}

RS_Vector LC_SplinePoints::getNearestCenter(const RS_Vector& /*coord*/,
	double* dist)
{
	if(dist != NULL)
	{
		*dist = RS_MAXDOUBLE;
	}

	return RS_Vector(false);
}

RS_Vector GetNearestMiddleLine(const RS_Vector& x1, const RS_Vector& x2,
	const RS_Vector& coord, double* dist, int middlePoints)
{
	double dt = 1.0/(1.0 + middlePoints);
	RS_Vector vMiddle;
	RS_Vector vRes = x1*(1.0 - dt) + x2*dt;
	double dMinDist = (vRes - coord).magnitude();
	double dCurDist;

	for(int i = 1; i < middlePoints; i++)
	{
		dt = (1.0 + i)/(1.0 + middlePoints);
		vMiddle = x1*(1.0 - dt) + x2*dt;
		dCurDist = (vMiddle - coord).magnitude();

		if(dCurDist < dMinDist)
		{
			dMinDist = dCurDist;
			vRes = vMiddle;
		}
	}

	if(dist) *dist = dMinDist;
	return vRes;
}

RS_Vector LC_SplinePoints::GetSplinePointAtDist(double dDist, int iStartSeg,
	double dStartT, int *piSeg, double *pdt) const
{
	RS_Vector vRes(false);

	RS_Vector vStart(false), vControl(false), vEnd(false);

	int iPoints = data.splinePoints.count();
	int n = data.controlPoints.count();
	int i = iStartSeg;

	if(i < 1) vStart = data.splinePoints.at(0);
	else vStart = (data.controlPoints.at(i - 1) + data.controlPoints.at(i))/2.0;
	vControl = data.controlPoints.at(i);
	if(i < n - 1) vEnd = (data.controlPoints.at(i) + data.controlPoints.at(i + 1))/2.0;
	else vEnd = data.splinePoints.at(iPoints - 1);

	double dQuadDist = GetQuadLength(vStart, vControl, vEnd, dStartT, 1.0);
	i++;

	while(dDist > dQuadDist && i < n - 1)
	{
		dDist -= dQuadDist;
		vStart = vEnd;
		vControl = data.controlPoints.at(i);
		vEnd = (data.controlPoints.at(i) + data.controlPoints.at(i + 1))/2.0;
		dQuadDist = GetQuadLength(vStart, vControl, vEnd, 0.0, 1.0);
		i++;
	}

	if(dDist > dQuadDist)
	{
		dDist -= dQuadDist;
		vStart = vEnd;
		vControl = data.controlPoints.at(n - 1);
		vEnd = data.splinePoints.at(iPoints - 1);
		dQuadDist = GetQuadLength(vStart, vControl, vEnd, 0.0, 1.0);
		i++;
	}

	if(dDist <= dQuadDist)
	{
		double dt = GetQuadPointAtDist(vStart, vControl, vEnd, 0.0, dDist);
		vRes = GetQuadPoint(vStart, vControl, vEnd, dt);
		*piSeg = i - 1;
		*pdt = dt;
	}

	return vRes;
}

RS_Vector LC_SplinePoints::getNearestMiddle(const RS_Vector& coord,
	double* dist, int middlePoints) const
{
	if(dist) *dist = RS_MAXDOUBLE;
	RS_Vector vRes(false);
	if(middlePoints < 1) return vRes;

	int iPoints = data.splinePoints.count();

	if(iPoints < 1) return RS_Vector(false);

	RS_Vector vStart = data.splinePoints.at(0);

	if(iPoints < 2)
	{
		if(dist) *dist = (vStart - coord).magnitude();
		return vStart;
	}

	RS_Vector vControl(false), vEnd(false), vNext(false);
	int i;
	double dMinDist, dCurDist, dt;
	dMinDist = RS_MAXDOUBLE;

	if(iPoints < 3)
	{
		vEnd = data.splinePoints.at(1);
		vRes = GetNearestMiddleLine(vStart, vEnd, coord, dist, middlePoints);
		return vRes;
	}

	//UpdateControlPoints();
	double dDist = getLength()/(1.0 + middlePoints);

	if(!data.closed)
	{
		if(iPoints == 3)
		{
			vStart = data.splinePoints.at(0);
			vEnd = data.splinePoints.at(2);
			vControl = GetThreePointsControl(vStart, data.splinePoints.at(1), vEnd);

			if(vControl.valid)
			{
				dt = GetQuadPointAtDist(vStart, vControl, vEnd, 0.0, dDist);
				vRes = GetQuadPoint(vStart, vControl, vEnd, dt);
				dMinDist = (vRes - coord).magnitude();
				for(int j = 1; j < middlePoints; j++)
				{
					dt = GetQuadPointAtDist(vStart, vControl, vEnd, dt, dDist);
					vNext = GetQuadPoint(vStart, vControl, vEnd, dt);
					dCurDist = (vNext - coord).magnitude();

					if(dCurDist < dMinDist)
					{
						dMinDist = dCurDist;
						vRes = vNext;
					}
				}
				if(dist) *dist = dMinDist;
			}
			else
			{
				vRes = GetNearestMiddleLine(vStart, vEnd, coord, dist, middlePoints);
			}

			return vRes;
		}

		int iNext;
		vRes = GetSplinePointAtDist(dDist, 0, 0.0, &iNext, &dt);
		if(vRes.valid) dMinDist = (vRes - coord).magnitude();
		i = 1;
		while(vRes.valid && i < middlePoints)
		{
			vNext = GetSplinePointAtDist(dDist, iNext, dt, &iNext, &dt);
			dCurDist = (vNext - coord).magnitude();

			if(vNext.valid && dCurDist < dMinDist)
			{
				dMinDist = dCurDist;
				vRes = vNext;
			}
			i++;
		}

		if(dist) *dist = dMinDist;
	}

	return vRes;
}

RS_Vector LC_SplinePoints::getNearestDist(double /*distance*/,
	const RS_Vector& /*coord*/, double* dist)
{
printf("getNearestDist\n");
	if(dist != NULL)
	{
		*dist = RS_MAXDOUBLE;
	}

	return RS_Vector(false);
}

void LC_SplinePoints::move(const RS_Vector& offset)
{
	for(int i = 0; i < data.splinePoints.count(); i++)
	{
		data.splinePoints[i].move(offset);
	}
	update();
}

void LC_SplinePoints::rotate(const RS_Vector& center, const double& angle)
{
	rotate(center, RS_Vector(angle));
}

void LC_SplinePoints::rotate(const RS_Vector& center, const RS_Vector& angleVector)
{
	for(int i = 0; i < data.splinePoints.count(); i++)
	{
		data.splinePoints[i].rotate(center, angleVector);
	}
	update();
}

void LC_SplinePoints::scale(const RS_Vector& center, const RS_Vector& factor)
{
	for(int i = 0; i < data.splinePoints.count(); i++)
	{
		data.splinePoints[i].scale(center, factor);
	}
	update();
}

void LC_SplinePoints::mirror(const RS_Vector& axisPoint1, const RS_Vector& axisPoint2)
{
	for(int i = 0; i < data.splinePoints.count(); i++)
	{
		data.splinePoints[i].mirror(axisPoint1, axisPoint2);
	}
	update();
}

void LC_SplinePoints::moveRef(const RS_Vector& ref, const RS_Vector& offset)
{
	for(int i = 0; i < data.splinePoints.count(); i++)
	{
		if(ref.distanceTo(data.splinePoints.at(i)) < 1.0e-4)
		{
			data.splinePoints[i].move(offset);
		}
	}
	update();
}

void LC_SplinePoints::revertDirection()
{
	for(int k = 0; k < data.splinePoints.count() / 2; k++)
	{
		data.splinePoints.swap(k, data.splinePoints.size() - 1 - k);
	}
	UpdateControlPoints();
}

/**
 * @return The reference points of the spline.
 */
QList<RS_Vector> LC_SplinePoints::getPoints()
{
	return data.splinePoints;
}

/**
 * Appends the given point to the control points.
 */
bool LC_SplinePoints::addPoint(const RS_Vector& v)
{
	if(data.splinePoints.count() < 1 ||
		(v - data.splinePoints.last()).squared() > RS_TOLERANCE2)
	{
		data.splinePoints.append(v);
		return true;
	}
	return false;
}

/**
 * Removes the control point that was last added.
 */
void LC_SplinePoints::removeLastPoint()
{
	data.splinePoints.pop_back();
}

double* GetMatrix(int iCount, bool bClosed, double *dt)
{
	if(bClosed && iCount < 3) return NULL;
	if(!bClosed && iCount < 4) return NULL;

	int iDim = 0;
	if(bClosed) iDim = 5*iCount - 6; // n + 2*(n - 1) + 2*(n - 2)
	else iDim = 3*iCount - 8; // (n - 2) + 2*(n - 3)

	double *dRes = new double[iDim];

	double x1, x2, x3;

	if(bClosed)
	{
		double *pdDiag = dRes;
		double *pdDiag1 = &dRes[iCount];
		double *pdDiag2 = &dRes[2*iCount - 1];
		double *pdLastCol1 = &dRes[3*iCount - 2];
		double *pdLastCol2 = &dRes[4*iCount - 4];

		x1 = (1.0 - dt[0])*(1.0 - dt[0])/2.0;
		x3 = dt[0]*dt[0]/2.0;
		x2 = x1 + 2.0*dt[0]*(1.0 - dt[0]) + x3;

		pdDiag[0] = sqrt(x2);
		pdDiag1[0] = x3/pdDiag[0];
		pdLastCol1[0] = x1/pdDiag[0];

		x1 = (1.0 - dt[1])*(1.0 - dt[1])/2.0;
		x3 = dt[1]*dt[1]/2.0;
		x2 = x1 + 2.0*dt[1]*(1.0 - dt[1]) + x3;

		pdDiag2[0] = x1/pdDiag[0];

		pdDiag[1] = sqrt(x2 - pdDiag1[0]*pdDiag2[0]);
		pdDiag1[1] = x3/pdDiag[1];
		pdLastCol1[1] = -pdDiag2[0]*pdLastCol1[0]/pdDiag[1];

		for(int i = 2; i < iCount - 2; i++)
		{
			x1 = (1.0 - dt[i])*(1.0 - dt[i])/2.0;
			x3 = dt[i]*dt[i]/2.0;
			x2 = x1 + 2.0*dt[i]*(1.0 - dt[i]) + x3;

			pdDiag2[i - 1] = x1/pdDiag[i - 1];

			pdDiag[i] = sqrt(x2 - pdDiag1[i - 1]*pdDiag2[i - 1]);
			pdDiag1[i] = x3/pdDiag[i];
			pdLastCol1[i] = -pdDiag2[i - 1]*pdLastCol1[i - 1]/pdDiag[i];
		}
		x1 = (1.0 - dt[iCount - 2])*(1.0 - dt[iCount - 2])/2.0;
		x3 = dt[iCount - 2]*dt[iCount - 2]/2.0;
		x2 = x1 + 2.0*dt[iCount - 2]*(1.0 - dt[iCount - 2]) + x3;

		pdDiag2[iCount - 3] = x1/pdDiag[iCount - 3];

		pdDiag[iCount - 2] = sqrt(x2 - pdDiag1[iCount - 3]*pdDiag2[iCount - 3]);
		pdDiag1[iCount - 2] = (x3 - pdDiag2[iCount - 3]*pdLastCol1[iCount - 3])/pdDiag[iCount - 2];

		x1 = (1.0 - dt[iCount - 1])*(1.0 - dt[iCount - 1])/2.0;
		x3 = dt[iCount - 1]*dt[iCount - 1]/2.0;
		x2 = x1 + 2.0*dt[iCount - 1]*(1.0 - dt[iCount - 1]) + x3;

		pdLastCol2[0] = x3/pdDiag[0];
		double dLastColSum = pdLastCol1[0]*pdLastCol2[0];
		for(int i = 1; i < iCount - 2; i++)
		{
			pdLastCol2[i] = -pdLastCol2[i - 1]*pdDiag1[i - 1]/pdDiag[i];
			dLastColSum += pdLastCol1[i]*pdLastCol2[i];
		}

		pdDiag2[iCount - 2] = (x1 - pdDiag1[iCount - 3]*pdLastCol2[iCount - 3])/pdDiag[iCount - 2];

		dLastColSum += pdDiag1[iCount - 2]*pdDiag2[iCount - 2];
		pdDiag[iCount - 1] = sqrt(x2 - dLastColSum);
	}
	else
	{
		double *pdDiag = dRes;
		double *pdDiag1 = &dRes[iCount - 2];
		double *pdDiag2 = &dRes[2*iCount - 5];

		x3 = dt[0]*dt[0]/2.0;
		x2 = 2.0*dt[0]*(1.0 - dt[0]) + x3;
		pdDiag[0] = sqrt(x2);
		pdDiag1[0] = x3/pdDiag[0];

		for(int i = 1; i < iCount - 3; i++)
		{
			x1 = (1.0 - dt[i])*(1.0 - dt[i])/2.0;
			x3 = dt[i]*dt[i]/2.0;
			x2 = x1 + 2.0*dt[i]*(1.0 - dt[i]) + x3;

			pdDiag2[i - 1] = x1/pdDiag[i - 1];
			pdDiag[i] = sqrt(x2 - pdDiag1[i - 1]*pdDiag2[i - 1]);
			pdDiag1[i] = x3/pdDiag[i];
		}

		x1 = (1.0 - dt[iCount - 3])*(1.0 - dt[iCount - 3])/2.0;
		x2 = x1 + 2.0*dt[iCount - 3]*(1.0 - dt[iCount - 3]);
		pdDiag2[iCount - 4] = x1/pdDiag[iCount - 4];
		pdDiag[iCount - 3] = sqrt(x2 - pdDiag1[iCount - 4]*pdDiag2[iCount - 4]);
	}

	return(dRes);
}

void LC_SplinePoints::UpdateControlPoints()
{
	data.controlPoints.clear();

	int n = data.splinePoints.count();

	if(data.closed && n < 3) return;
	if(!data.closed && n < 4) return;

	int iDim = 0;
	if(data.closed) iDim = n;
	else iDim = n - 2;

	double *dt = new double[iDim];
	double dl1, dl2;

	if(data.closed)
	{
		dl1 = (data.splinePoints.at(n - 1) - data.splinePoints.at(0)).magnitude();
		dl2 = (data.splinePoints.at(1) - data.splinePoints.at(0)).magnitude();
		dt[0] = dl1/(dl1 + dl2);
		for(int i = 1; i < iDim - 1; i++)
		{
			dl1 = dl2;
			dl2 = (data.splinePoints.at(i + 1) - data.splinePoints.at(i)).magnitude();
			dt[i] = dl1/(dl1 + dl2);
		}
		dl1 = (data.splinePoints.at(n - 1) - data.splinePoints.at(n - 2)).magnitude();
		dl2 = (data.splinePoints.at(0) - data.splinePoints.at(n - 1)).magnitude();
		dt[iDim - 1] = dl1/(dl1 + dl2);
	}
	else
	{
		dl1 = (data.splinePoints.at(1) - data.splinePoints.at(0)).magnitude();
		dl2 = (data.splinePoints.at(2) - data.splinePoints.at(1)).magnitude();
		dt[0] = dl1/(dl1 + dl2/2.0);
		for(int i = 1; i < iDim - 1; i++)
		{
			dl1 = dl2;
			dl2 = (data.splinePoints.at(i + 2) - data.splinePoints.at(i + 1)).magnitude();
			dt[i] = dl1/(dl1 + dl2);
		}
		dl1 = dl2;
		dl2 = (data.splinePoints.at(iDim) - data.splinePoints.at(iDim + 1)).magnitude();
		dt[iDim - 1] = dl1/(dl1 + 2.0*dl2);
	}

	double *pdMatrix = GetMatrix(n, data.closed, dt);

	if(!pdMatrix) return;

	double *dx = new double[iDim];
	double *dy = new double[iDim];
	double *dx2 = new double[iDim];
	double *dy2 = new double[iDim];

	if(data.closed)
	{
		double *pdDiag = pdMatrix;
		double *pdDiag1 = &pdMatrix[n];
		double *pdDiag2 = &pdMatrix[2*n - 1];
		double *pdLastCol1 = &pdMatrix[3*n - 2];
		double *pdLastCol2 = &pdMatrix[4*n - 4];

		dx[0] = data.splinePoints.at(0).x/pdDiag[0];
		dy[0] = data.splinePoints.at(0).y/pdDiag[0];
		for(int i = 1; i < iDim - 1; i++)
		{
			dx[i] = (data.splinePoints.at(i).x - pdDiag2[i - 1]*dx[i - 1])/pdDiag[i];
			dy[i] = (data.splinePoints.at(i).y - pdDiag2[i - 1]*dy[i - 1])/pdDiag[i];
		}

		dx[iDim - 1] = data.splinePoints.at(iDim - 1).x - pdDiag2[iDim - 2]*dx[iDim - 2];
		dy[iDim - 1] = data.splinePoints.at(iDim - 1).y - pdDiag2[iDim - 2]*dy[iDim - 2];
		for(int i = 0; i < iDim - 2; i++)
		{
			dx[iDim - 1] -= (dx[i]*pdLastCol2[i]);
			dy[iDim - 1] -= (dy[i]*pdLastCol2[i]);
		}
		dx[iDim - 1] /= pdDiag[iDim - 1];
		dy[iDim - 1] /= pdDiag[iDim - 1];

		dx2[iDim - 1] = dx[iDim - 1]/pdDiag[iDim - 1];
		dy2[iDim - 1] = dy[iDim - 1]/pdDiag[iDim - 1];
		dx2[iDim - 2] = (dx[iDim - 2] - pdDiag1[iDim - 2]*dx2[iDim - 1])/pdDiag[iDim - 2];
		dy2[iDim - 2] = (dy[iDim - 2] - pdDiag1[iDim - 2]*dy2[iDim - 1])/pdDiag[iDim - 2];

		for(int i = iDim - 3; i >= 0; i--)
		{
			dx2[i] = (dx[i] - pdDiag1[i]*dx2[i + 1] - pdLastCol1[i]*dx2[iDim - 1])/pdDiag[i];
			dy2[i] = (dy[i] - pdDiag1[i]*dy2[i + 1] - pdLastCol1[i]*dy2[iDim - 1])/pdDiag[i];
		}

		for(int i = 0; i < iDim; i++)
		{
			data.controlPoints.append(RS_Vector(dx2[i], dy2[i]));
		}
	}
	else
	{
		double *pdDiag = pdMatrix;
		double *pdDiag1 = &pdMatrix[n - 2];
		double *pdDiag2 = &pdMatrix[2*n - 5];

		dx[0] = (data.splinePoints.at(1).x - data.splinePoints.at(0).x*(1.0 - dt[0])*(1.0 - dt[0]))/pdDiag[0];
		dy[0] = (data.splinePoints.at(1).y - data.splinePoints.at(0).y*(1.0 - dt[0])*(1.0 - dt[0]))/pdDiag[0];
		for(int i = 1; i < iDim - 1; i++)
		{
			dx[i] = (data.splinePoints.at(i + 1).x - pdDiag2[i - 1]*dx[i - 1])/pdDiag[i];
			dy[i] = (data.splinePoints.at(i + 1).y - pdDiag2[i - 1]*dy[i - 1])/pdDiag[i];
		}
		dx[iDim - 1] = ((data.splinePoints.at(iDim).x - data.splinePoints.at(iDim + 1).x*dt[n - 3]*dt[n - 3]) -
			pdDiag2[iDim - 2]*dx[iDim - 2])/pdDiag[iDim - 1];
		dy[iDim - 1] = ((data.splinePoints.at(iDim).y - data.splinePoints.at(iDim + 1).y*dt[n - 3]*dt[n - 3]) -
			pdDiag2[iDim - 2]*dy[iDim - 2])/pdDiag[iDim - 1];

		dx2[iDim - 1] = dx[iDim - 1]/pdDiag[iDim - 1];
		dy2[iDim - 1] = dy[iDim - 1]/pdDiag[iDim - 1];

		for(int i = iDim - 2; i >= 0; i--)
		{
			dx2[i] = (dx[i] - pdDiag1[i]*dx2[i + 1])/pdDiag[i];
			dy2[i] = (dy[i] - pdDiag1[i]*dy2[i + 1])/pdDiag[i];
		}

		for(int i = 0; i < iDim; i++)
		{
			data.controlPoints.append(RS_Vector(dx2[i], dy2[i]));
		}
	}

	delete[] pdMatrix;

	delete[] dt;

	delete[] dy2;
	delete[] dx2;
	delete[] dy;
	delete[] dx;
}

double GetLinePointAtDist(double dLen, double t1, double dDist)
{
	return t1 + dDist/dLen;
}

// returns new pattern offset;
double DrawPatternLine(double *pdPattern, int iPattern, double patternOffset,
	QPainterPath& qPath, RS_Vector& x1, RS_Vector& x2)
{
	double dLen = (x2 - x1).magnitude();
	if(dLen < RS_TOLERANCE) return(patternOffset);

	int i = 0;
	double dCurSegLen = 0.0;
	double dSegOffs = 0.0;
	while(patternOffset > RS_TOLERANCE)
	{
		if(i >= iPattern) i = 0;
		dCurSegLen = fabs(pdPattern[i++]);
		if(patternOffset > dCurSegLen) patternOffset -= dCurSegLen;
		else
		{
			dSegOffs = patternOffset;
			patternOffset = 0.0;
		}
	}
	if(i > 0) i--;

	dCurSegLen = fabs(pdPattern[i]) - dSegOffs;
	dSegOffs = 0.0;

	double dt1 = 0.0;
	double dt2 = 1.0;
	double dCurLen = dLen;
	if(dCurSegLen < dCurLen)
	{
		dt2 = GetLinePointAtDist(dLen, dt1, dCurSegLen);
		dCurLen -= dCurSegLen;
	}
	else 
	{
		dSegOffs = dCurLen;
		dCurLen = 0.0;
	}
	
    RS_Vector p2;

	p2 = x1*(1.0 - dt2) + x2*dt2;
	if(pdPattern[i] < 0) qPath.moveTo(QPointF(p2.x, p2.y));
	else qPath.lineTo(QPointF(p2.x, p2.y));

	i++;
	dt1 = dt2;

	while(dCurLen > RS_TOLERANCE)
	{
		if(i >= iPattern) i = 0;

		dCurSegLen = fabs(pdPattern[i]);
		if(dCurLen > dCurSegLen)
		{
			dt2 = GetLinePointAtDist(dLen, dt1, dCurSegLen);
			dCurLen -= dCurSegLen;
		}
		else
		{
			dt2 = 1.0;
			dSegOffs = dCurLen;
			dCurLen = 0.0;
		}

		p2 = x1*(1.0 - dt2) + x2*dt2;
		if(pdPattern[i] < 0) qPath.moveTo(QPointF(p2.x, p2.y));
		else qPath.lineTo(QPointF(p2.x, p2.y));

		i++;
		dt1 = dt2;
	}

	i--;

	while(i > 0)
	{
		dSegOffs += fabs(pdPattern[--i]);
	}
	return(dSegOffs);
}

// returns new pattern offset;
double DrawPatternQuad(double *pdPattern, int iPattern, double patternOffset,
	QPainterPath& qPath, RS_Vector& x1, RS_Vector& c1, RS_Vector& x2)
{
	double dLen = GetQuadLength(x1, c1, x2, 0.0, 1.0);
	if(dLen < RS_TOLERANCE) return(patternOffset);

	int i = 0;
	double dCurSegLen = 0.0;
	double dSegOffs = 0.0;
	while(patternOffset > RS_TOLERANCE)
	{
		if(i >= iPattern) i = 0;
		dCurSegLen = fabs(pdPattern[i++]);
		if(patternOffset > dCurSegLen) patternOffset -= dCurSegLen;
		else
		{
			dSegOffs = patternOffset;
			patternOffset = 0.0;
		}
	}
	if(i > 0) i--;

	dCurSegLen = fabs(pdPattern[i]) - dSegOffs;
	dSegOffs = 0.0;

	double dt1 = 0.0;
	double dt2 = 1.0;
	double dCurLen = dLen;
	if(dCurSegLen < dCurLen)
	{
		dt2 = GetQuadPointAtDist(x1, c1, x2, dt1, dCurSegLen);
		dCurLen -= dCurSegLen;
	}
	else 
	{
		dSegOffs = dCurLen;
		dCurLen = 0.0;
	}
	
    RS_Vector c2;
    RS_Vector p2;

	p2 = GetQuadPoint(x1, c1, x2, dt2);
	if(pdPattern[i] < 0) qPath.moveTo(QPointF(p2.x, p2.y));
	else
	{
		c2 = GetSubQuadControlPoint(x1, c1, x2, dt1, dt2);
		qPath.quadTo(QPointF(c2.x, c2.y), QPointF(p2.x, p2.y));
	}

	i++;
	dt1 = dt2;

	while(dCurLen > RS_TOLERANCE)
	{
		if(i >= iPattern) i = 0;

		dCurSegLen = fabs(pdPattern[i]);
		if(dCurLen > dCurSegLen)
		{
			dt2 = GetQuadPointAtDist(x1, c1, x2, dt1, dCurSegLen);
			dCurLen -= dCurSegLen;
		}
		else
		{
			dt2 = 1.0;
			dSegOffs = dCurLen;
			dCurLen = 0.0;
		}

		p2 = GetQuadPoint(x1, c1, x2, dt2);
		if(pdPattern[i] < 0) qPath.moveTo(QPointF(p2.x, p2.y));
		else
		{
			c2 = GetSubQuadControlPoint(x1, c1, x2, dt1, dt2);
			qPath.quadTo(QPointF(c2.x, c2.y), QPointF(p2.x, p2.y));
		}

		i++;
		dt1 = dt2;
	}

	i--;

	while(i > 0)
	{
		dSegOffs += fabs(pdPattern[--i]);
	}
	return(dSegOffs);
}

void LC_SplinePoints::drawPattern(RS_Painter* painter, RS_GraphicView* view,
	int iPoints, double& patternOffset, RS_LineTypePattern* pat)
{
	double dpmm = static_cast<RS_PainterQt*>(painter)->getDpmm();
	double* ds = new double[pat->num];
	for(int i = 0; i < pat->num; i++)
	{
		ds[i] = dpmm*pat->pattern[i];
		if(fabs(ds[i]) < 1.0) ds[i] = (ds[i] >= 0.0) ? 1.0 : -1.0;
	}

	RS_Vector vStart = data.splinePoints.at(0);
	RS_Vector vControl(false), vEnd(false);

	RS_Vector vx1, vc1, vx2;
	vx1 = view->toGui(vStart);

	QPainterPath qPath(QPointF(vx1.x, vx1.y));
	double dCurOffset = dpmm*patternOffset;

	if(iPoints < 3)
	{
		if(iPoints > 1)
		{
			vx2 = view->toGui(data.splinePoints.at(1));
			DrawPatternLine(ds, pat->num, dCurOffset, qPath, vx1, vx2);
			painter->drawPath(qPath);
		}
		delete[] ds;
		return;
	}

	int n = data.controlPoints.count();

	if(data.closed)
	{
		vStart = (data.controlPoints.at(n - 1) + data.controlPoints.at(0))/2.0;
		vx1 = view->toGui(vStart);
		qPath.moveTo(QPointF(vx1.x, vx1.y));

		vControl = data.controlPoints.at(0);
		vEnd = (data.controlPoints.at(0) + data.controlPoints.at(1))/2.0;
		vc1 = view->toGui(vControl);
		vx2 = view->toGui(vEnd);
		dCurOffset = DrawPatternQuad(ds, pat->num, dCurOffset, qPath, vx1, vc1, vx2);

		for(int i = 1; i < n - 1; i++)
		{
			vx1 = vx2;
			vControl = data.controlPoints.at(i);
			vEnd = (data.controlPoints.at(i) + data.controlPoints.at(i + 1))/2.0;
			vc1 = view->toGui(vControl);
			vx2 = view->toGui(vEnd);
			dCurOffset = DrawPatternQuad(ds, pat->num, dCurOffset, qPath, vx1, vc1, vx2);
		}

		vx1 = vx2;
		vControl = data.controlPoints.at(n - 1);
		vEnd = (data.controlPoints.at(n - 1) + data.controlPoints.at(0))/2.0;
		vc1 = view->toGui(vControl);
		vx2 = view->toGui(vEnd);
		DrawPatternQuad(ds, pat->num, dCurOffset, qPath, vx1, vc1, vx2);
	}
	else
	{
		if(iPoints == 3)
		{
			vStart = data.splinePoints.at(0);
			vEnd = data.splinePoints.at(2);

			vx1 = view->toGui(vStart);
			vx2 = view->toGui(vEnd);

			vControl = GetThreePointsControl(vStart, data.splinePoints.at(1), vEnd);

			if(vControl.valid)
			{
				vc1 = view->toGui(vControl);
				DrawPatternQuad(ds, pat->num, dCurOffset, qPath, vx1, vc1, vx2);
			}
			else DrawPatternLine(ds, pat->num, dCurOffset, qPath, vx1, vx2);

			painter->drawPath(qPath);
			delete[] ds;
			return;
		}

		vControl = data.controlPoints.at(0);
		vEnd = (data.controlPoints.at(0) + data.controlPoints.at(1))/2.0;
		vc1 = view->toGui(vControl);
		vx2 = view->toGui(vEnd);
		dCurOffset = DrawPatternQuad(ds, pat->num, dCurOffset, qPath, vx1, vc1, vx2);

		for(int i = 1; i < n - 1; i++)
		{
			vx1 = vx2;
			vControl = data.controlPoints.at(i);
			vEnd = (data.controlPoints.at(i) + data.controlPoints.at(i + 1))/2.0;
			vc1 = view->toGui(vControl);
			vx2 = view->toGui(vEnd);
			dCurOffset = DrawPatternQuad(ds, pat->num, dCurOffset, qPath, vx1, vc1, vx2);
		}

		vx1 = vx2;
		vControl = data.controlPoints.at(n - 1);
		vEnd = data.splinePoints.at(iPoints - 1);
		vc1 = view->toGui(vControl);
		vx2 = view->toGui(vEnd);
		DrawPatternQuad(ds, pat->num, dCurOffset, qPath, vx1, vc1, vx2);
	}

	painter->drawPath(qPath);
	delete[] ds;
}

void LC_SplinePoints::drawSimple(RS_Painter* painter, RS_GraphicView* view, int iPoints)
{
	RS_Vector vStart = view->toGui(data.splinePoints.at(0));
	RS_Vector vControl(false), vEnd(false);

	QPainterPath qPath(QPointF(vStart.x, vStart.y));

	if(iPoints < 3)
	{
		if(iPoints > 1)
		{
			vEnd = view->toGui(data.splinePoints.at(1));
			qPath.lineTo(QPointF(vEnd.x, vEnd.y));
			painter->drawPath(qPath);
		}
		return;
	}

	int n = data.controlPoints.count();

	if(data.closed)
	{
		vStart = (data.controlPoints.at(n - 1) + data.controlPoints.at(0))/2.0;
		vControl = view->toGui(vStart);
		qPath.moveTo(QPointF(vControl.x, vControl.y));

		vControl = data.controlPoints.at(0);
		vEnd = (data.controlPoints.at(0) + data.controlPoints.at(1))/2.0;
		vStart = view->toGui(vControl);
		vControl = view->toGui(vEnd);
		qPath.quadTo(QPointF(vStart.x, vStart.y), QPointF(vControl.x, vControl.y));

		for(int i = 1; i < n - 1; i++)
		{
			vControl = data.controlPoints.at(i);
			vEnd = (data.controlPoints.at(i) + data.controlPoints.at(i + 1))/2.0;
			vStart = view->toGui(vControl);
			vControl = view->toGui(vEnd);
			qPath.quadTo(QPointF(vStart.x, vStart.y), QPointF(vControl.x, vControl.y));
		}

		vControl = data.controlPoints.at(n - 1);
		vEnd = (data.controlPoints.at(n - 1) + data.controlPoints.at(0))/2.0;
		vStart = view->toGui(vControl);
		vControl = view->toGui(vEnd);
		qPath.quadTo(QPointF(vStart.x, vStart.y), QPointF(vControl.x, vControl.y));
	}
	else
	{
		if(iPoints == 3)
		{
			vStart = data.splinePoints.at(0);
			vEnd = data.splinePoints.at(2);

			vControl = GetThreePointsControl(vStart, data.splinePoints.at(1), vEnd);

			if(vControl.valid)
			{
				vStart = view->toGui(vControl);
				vControl = view->toGui(vEnd);
				qPath.quadTo(QPointF(vStart.x, vStart.y), QPointF(vControl.x, vControl.y));
			}
			else
			{
				vControl = view->toGui(vEnd);
				qPath.lineTo(QPointF(vControl.x, vControl.y));
			}

			painter->drawPath(qPath);
			return;
		}

		vControl = data.controlPoints.at(0);
		vEnd = (data.controlPoints.at(0) + data.controlPoints.at(1))/2.0;
		vStart = view->toGui(vControl);
		vControl = view->toGui(vEnd);
		qPath.quadTo(QPointF(vStart.x, vStart.y), QPointF(vControl.x, vControl.y));

		for(int i = 1; i < n - 1; i++)
		{
			vControl = data.controlPoints.at(i);
			vEnd = (data.controlPoints.at(i) + data.controlPoints.at(i + 1))/2.0;
			vStart = view->toGui(vControl);
			vControl = view->toGui(vEnd);
			qPath.quadTo(QPointF(vStart.x, vStart.y), QPointF(vControl.x, vControl.y));
		}

		vControl = data.controlPoints.at(n - 1);
		vEnd = data.splinePoints.at(iPoints - 1);
		vStart = view->toGui(vControl);
		vControl = view->toGui(vEnd);
		qPath.quadTo(QPointF(vStart.x, vStart.y), QPointF(vControl.x, vControl.y));
	}

	painter->drawPath(qPath);
}

void LC_SplinePoints::draw(RS_Painter* painter, RS_GraphicView* view, double& patternOffset)
{
	if(painter == NULL || view == NULL)
	{
		return;
	}

	int iPoints = data.splinePoints.count();

	if(iPoints < 2) return;

	// Pattern:
	RS_LineTypePattern* pat = NULL;
	if(isSelected())
	{
//		styleFactor=1.;
		pat = &patternSelected;
	}
	else
	{
		pat = view->getPattern(getPen().getLineType());
	}

	bool bDrawPattern = false;
	if(pat) bDrawPattern = pat->num > 0;
	else
	{
		RS_DEBUG->print(RS_Debug::D_WARNING,
			"RS_Line::draw: Invalid line pattern");
	}

	UpdateControlPoints();
	calculateBorders();

    // Pen to draw pattern is always solid:
    RS_Pen pen = painter->getPen();
    pen.setLineType(RS2::SolidLine);
    painter->setPen(pen);

	if(bDrawPattern)
		drawPattern(painter, view, iPoints, patternOffset, pat);
	else drawSimple(painter, view, iPoints);
}

double LC_SplinePoints::getLength() const
{
	int iPoints = data.splinePoints.count();

	RS_Vector vStart(false), vControl(false), vEnd(false);

	if(iPoints < 2) return 0;

	//UpdateControlPoints();

	if(iPoints < 3) return sqrt((vEnd - vStart).squared());

	int n = data.controlPoints.count();

	double dRes = 0.0;

	if(data.closed)
	{
		vStart = (data.controlPoints.at(n - 1) + data.controlPoints.at(0))/2.0;
		vControl = data.controlPoints.at(0);
		vEnd = (data.controlPoints.at(0) + data.controlPoints.at(1))/2.0;

		dRes = GetQuadLength(vStart, vControl, vEnd, 0.0, 1.0);

		for(int i = 1; i < n - 1; i++)
		{
			vStart = vEnd;
			vControl = data.controlPoints.at(i);
			vEnd = (data.controlPoints.at(i) + data.controlPoints.at(i + 1))/2.0;

			dRes += GetQuadLength(vStart, vControl, vEnd, 0.0, 1.0);
		}

		vStart = vEnd;
		vControl = data.controlPoints.at(n - 1);
		vEnd = (data.controlPoints.at(n - 1) + data.controlPoints.at(0))/2.0;

		dRes += GetQuadLength(vStart, vControl, vEnd, 0.0, 1.0);
	}
	else
	{
		if(iPoints == 3)
		{
			vStart = data.splinePoints.at(0);
			vEnd = data.splinePoints.at(2);
			vControl = GetThreePointsControl(vStart, data.splinePoints.at(1), vEnd);

			if(vControl.valid) dRes = GetQuadLength(vStart, vControl, vEnd, 0.0, 1.0);
			else dRes = (vEnd - vStart).magnitude();

			return dRes;
		}

		vStart = data.splinePoints.at(0);
		vControl = data.controlPoints.at(0);
		vEnd = (data.controlPoints.at(0) + data.controlPoints.at(1))/2.0;

		dRes = GetQuadLength(vStart, vControl, vEnd, 0.0, 1.0);

		for(int i = 1; i < n - 1; i++)
		{
			vStart = vEnd;
			vControl = data.controlPoints.at(i);
			vEnd = (data.controlPoints.at(i) + data.controlPoints.at(i + 1))/2.0;

			dRes += GetQuadLength(vStart, vControl, vEnd, 0.0, 1.0);
		}

		vStart = vEnd;
		vControl = data.controlPoints.at(n - 1);
		vEnd = data.splinePoints.at(iPoints - 1);

		dRes += GetQuadLength(vStart, vControl, vEnd, 0.0, 1.0);
	}

	return dRes;
}

double LC_SplinePoints::getDirection1() const
{
	int iPoints = data.splinePoints.count();

	if(iPoints < 2) return 0.0;

	RS_Vector vStart, vEnd, vx1;

	if(iPoints < 3)
	{
		vStart = data.splinePoints.at(0);
		vEnd = data.splinePoints.at(1);
		return(vStart.angleTo(vEnd));
	}

	//UpdateControlPoints();

	int n = data.controlPoints.count();

	if(data.closed)
	{
		vStart = (data.controlPoints.at(n - 1) + data.controlPoints.at(0))/2.0;
		vEnd = data.controlPoints.at(0);
	}
	else
	{
		if(iPoints == 3)
		{
			vStart = data.splinePoints.at(0);
			vx1 = data.splinePoints.at(2);
			vEnd = GetThreePointsControl(vStart, data.splinePoints.at(1), vx1);
		}
		else
		{
			vStart = data.splinePoints.at(0);
			vEnd = data.controlPoints.at(0);
		}
	}

	return(vStart.angleTo(vEnd));
}

double LC_SplinePoints::getDirection2() const
{
	int iPoints = data.splinePoints.count();

	if(iPoints < 2) return 0.0;

	RS_Vector vStart, vEnd, vx1;

	if(iPoints < 3)
	{
		vStart = data.splinePoints.at(0);
		vEnd = data.splinePoints.at(1);
		return(vEnd.angleTo(vStart));
	}

	//UpdateControlPoints();

	int n = data.controlPoints.count();

	if(data.closed)
	{
		vStart = data.controlPoints.at(n - 1);
		vEnd = (data.controlPoints.at(n - 1) + data.controlPoints.at(0))/2.0;
	}
	else
	{
		if(iPoints == 3)
		{
			vx1 = data.splinePoints.at(0);
			vEnd = data.splinePoints.at(2);
			vStart = GetThreePointsControl(vx1, data.splinePoints.at(1), vEnd);
		}
		else
		{
			vStart = data.controlPoints.at(n - 1);
			vEnd = data.splinePoints.at(iPoints - 1);
		}
	}

	return(vEnd.angleTo(vStart));
}

void AddQuadTangentPoints(RS_VectorSolutions *pVS, const RS_Vector& point,
	const RS_Vector& x1, const RS_Vector& c1, const RS_Vector& x2)
{
	RS_Vector vx1 = x2 - c1*2.0 + x1;
	RS_Vector vx2 = c1 - x1;
	RS_Vector vx3 = x1 - point;

	double a1 = vx2.x*vx1.y - vx2.y*vx1.x;
	double a2 = vx3.x*vx1.y - vx3.y*vx1.x;
	double a3 = vx3.x*vx2.y - vx3.y*vx2.x;

	std::vector<double> dSol(0, 0.);

	if(fabs(a1) > RS_TOLERANCE)
	{
		std::vector<double> dCoefs(0, 0.);

		dCoefs.push_back(a2/a1);
		dCoefs.push_back(a3/a1);
		dSol = RS_Math::quadraticSolver(dCoefs);

	}
	else if(fabs(a2) > RS_TOLERANCE)
	{
		dSol.push_back(-a3/a2);
	}

    for(double& d: dSol)
	{
        if(d > -RS_TOLERANCE && d < 1.0 + RS_TOLERANCE)
		{
            if(d < 0.0) d = 0.0;
            if(d > 1.0) d = 1.0;
            pVS->push_back(GetQuadPoint(x1, c1, x2, d));
		}
	}
}

RS_VectorSolutions LC_SplinePoints::getTangentPoint(const RS_Vector& point) const
{
    RS_VectorSolutions ret;
	int iPoints = data.splinePoints.count();

	RS_Vector vStart(false), vControl(false), vEnd(false);

	if(iPoints < 3) return ret;

	//UpdateControlPoints();

	int n = data.controlPoints.count();

	if(data.closed)
	{
		vStart = (data.controlPoints.at(n - 1) + data.controlPoints.at(0))/2.0;
		vControl = data.controlPoints.at(0);
		vEnd = (data.controlPoints.at(0) + data.controlPoints.at(1))/2.0;

		AddQuadTangentPoints(&ret, point, vStart, vControl, vEnd);

		for(int i = 1; i < n - 1; i++)
		{
			vStart = vEnd;
			vControl = data.controlPoints.at(i);
			vEnd = (data.controlPoints.at(i) + data.controlPoints.at(i + 1))/2.0;

			AddQuadTangentPoints(&ret, point, vStart, vControl, vEnd);
		}

		vStart = vEnd;
		vControl = data.controlPoints.at(n - 1);
		vEnd = (data.controlPoints.at(n - 1) + data.controlPoints.at(0))/2.0;

		AddQuadTangentPoints(&ret, point, vStart, vControl, vEnd);
	}
	else
	{
		if(iPoints == 3)
		{
			vStart = data.splinePoints.at(0);
			vEnd = data.splinePoints.at(2);
			vControl = GetThreePointsControl(vStart, data.splinePoints.at(1), vEnd);

			if(vControl.valid)
				AddQuadTangentPoints(&ret, point, vStart, vControl, vEnd);

			return ret;
		}

		vStart = data.splinePoints.at(0);
		vControl = data.controlPoints.at(0);
		vEnd = (data.controlPoints.at(0) + data.controlPoints.at(1))/2.0;

		AddQuadTangentPoints(&ret, point, vStart, vControl, vEnd);

		for(int i = 1; i < n - 1; i++)
		{
			vStart = vEnd;
			vControl = data.controlPoints.at(i);
			vEnd = (data.controlPoints.at(i) + data.controlPoints.at(i + 1))/2.0;

			AddQuadTangentPoints(&ret, point, vStart, vControl, vEnd);
		}

		vStart = vEnd;
		vControl = data.controlPoints.at(n - 1);
		vEnd = data.splinePoints.at(iPoints - 1);

		AddQuadTangentPoints(&ret, point, vStart, vControl, vEnd);
	}

	return ret;
}

RS_Vector LC_SplinePoints::getTangentDirection(const RS_Vector& point) const
{
	int iPoints = data.splinePoints.count();

	RS_Vector vStart(false), vControl(false), vEnd(false), vRes(false);

	if(iPoints < 2) return vStart;

	double dt = 0.0;
	int iQuad = GetNearestQuad(point, NULL, &dt);
	if(iQuad < 0) return vStart;

	if(GetQuadPoints(iQuad, &vStart, &vControl, &vEnd))
		vRes = GetQuadDirAtPoint(vStart, vControl, vEnd, dt);
	else vRes = vEnd - vStart;

	return vRes;
}

LC_SplinePointsData AddLineOffset(const RS_Vector& vx1,
	const RS_Vector& vx2, double distance)
{
	LC_SplinePointsData ret(false);

	double dDist = (vx2 - vx1).magnitude();

	if(dDist < RS_TOLERANCE) return ret;

	dDist = distance/dDist;

	ret.splinePoints.append(RS_Vector(vx1.x - dDist*(vx2.y - vx1.y), vx1.y + dDist*(vx2.x - vx1.x)));
	ret.splinePoints.append(RS_Vector(vx2.x - dDist*(vx2.y - vx1.y), vx2.y + dDist*(vx2.x - vx1.x)));
	return ret;
}

bool LC_SplinePoints::offset(const RS_Vector& coord, const double& distance)
{
	int iPoints = data.splinePoints.count();
	if(iPoints < 2) return false;

	double dt;
	int iQuad = GetNearestQuad(coord, NULL, &dt);
	if(iQuad < 0) return false;

	RS_Vector vStart(false), vEnd(false), vControl(false);
	RS_Vector vPoint(false), vTan(false);

	if(GetQuadPoints(iQuad, &vStart, &vControl, &vEnd))
	{
		vPoint = GetQuadAtPoint(vStart, vControl, vEnd, dt);
		vTan = GetQuadDirAtPoint(vStart, vControl, vEnd, dt);
	}
	else
	{
		vPoint = vEnd*(1.0 - dt) - vStart*dt;
		vTan = vEnd - vStart;
	}

	double dDist = distance;
	if((coord.x - vPoint.x)*vTan.y - (coord.y - vPoint.y)*vTan.x > 0)
		dDist *= -1.0;

	LC_SplinePointsData spd(data.closed);

	bool bRes = false;

	if(iPoints < 3)
	{
		vStart = data.splinePoints.at(0);
		vEnd = data.splinePoints.at(1);
		spd = AddLineOffset(vStart, vEnd, dDist);
		bRes = spd.splinePoints.count() > 0;
		if(bRes) data = spd;
		return bRes;
	}

	int n = data.controlPoints.count();

	double dl1, dl2;

	if(data.closed)
	{
		vPoint = data.splinePoints.at(0);

		dl1 = (data.splinePoints.at(iPoints - 1) - vPoint).magnitude();
		dl2 = (data.splinePoints.at(1) - vPoint).magnitude();
		dt = dl1/(dl1 + dl2);

		vStart = (data.controlPoints.at(n - 1) + data.controlPoints.at(0))/2.0;
		vControl = data.controlPoints.at(0);
		vEnd = (data.controlPoints.at(0) + data.controlPoints.at(1))/2.0;

		vTan = GetQuadDir(vStart, vControl, vEnd, dt);
		if(vTan.valid)
		{
			spd.splinePoints.append(RS_Vector(vPoint.x - dDist*vTan.y,
				vPoint.y + dDist*vTan.x));
		}

		for(int i = 1; i < n - 1; i++)
		{
			vPoint = data.splinePoints.at(i);

			dl1 = dl2;
			dl2 = (data.splinePoints.at(i + 1) - vPoint).magnitude();
			dt = dl1/(dl1 + dl2);

			vStart = (data.controlPoints.at(i - 1) + data.controlPoints.at(i))/2.0;
			vControl = data.controlPoints.at(i);
			vEnd = (data.controlPoints.at(i) + data.controlPoints.at(i + 1))/2.0;

			vTan = GetQuadDir(vStart, vControl, vEnd, dt);

			if(vTan.valid)
			{
				spd.splinePoints.append(RS_Vector(vPoint.x - dDist*vTan.y,
					vPoint.y + dDist*vTan.x));
			}
		}

		vPoint = data.splinePoints.at(iPoints - 1);
		dl1 = (vPoint - data.splinePoints.at(iPoints - 2)).magnitude();
		dl2 = (vPoint - data.splinePoints.at(0)).magnitude();
		dt = dl1/(dl1 + dl2);

		vTan = GetQuadDir(vStart, vControl, vEnd, dt);
		if(vTan.valid)
		{
			spd.splinePoints.append(RS_Vector(vPoint.x - dDist*vTan.y,
				vPoint.y + dDist*vTan.x));
		}
	}
	else
	{
		if(iPoints < 4)
		{
			vStart = data.splinePoints.at(0);
			vTan = data.splinePoints.at(1);
			vEnd = data.splinePoints.at(2);
			dl1 = (vTan - vStart).magnitude();
			dl2 = (vEnd - vTan).magnitude();
			dt = dl1/(dl1 + dl2);

			if(dt < RS_TOLERANCE || dt > 1.0 - RS_TOLERANCE)
			{
				spd = AddLineOffset(vStart, vEnd, dDist);
				bRes = spd.splinePoints.count() > 0;
				if(bRes) data = spd;
				return bRes;
			}

			vControl = (vTan - vStart*(1.0 - dt)*(1.0 - dt) - vEnd*dt*dt)/dt/(1 - dt)/2.0;

			vPoint = vTan;
			vTan = GetQuadDir(vStart, vControl, vEnd, 0.0);
			if(vTan.valid)
			{
				spd.splinePoints.append(RS_Vector(vStart.x - dDist*vTan.y,
					vStart.y + dDist*vTan.x));
			}

			vTan = GetQuadDir(vStart, vControl, vEnd, dt);
			if(vTan.valid)
			{
				spd.splinePoints.append(RS_Vector(vPoint.x - dDist*vTan.y,
					vPoint.y + dDist*vTan.x));
			}

			vTan = GetQuadDir(vStart, vControl, vEnd, 1.0);
			if(vTan.valid)
			{
				spd.splinePoints.append(RS_Vector(vEnd.x - dDist*vTan.y,
					vEnd.y + dDist*vTan.x));
			}

			data = spd;
			return true;
		}

		vPoint = data.splinePoints.at(0);

		vStart = vPoint;
		vControl = data.controlPoints.at(0);
		vEnd = (data.controlPoints.at(0) + data.controlPoints.at(1))/2.0;

		vTan = GetQuadDir(vStart, vControl, vEnd, 0.0);
		if(vTan.valid)
		{
			spd.splinePoints.append(RS_Vector(vPoint.x - dDist*vTan.y,
				vPoint.y + dDist*vTan.x));
		}

		vPoint = data.splinePoints.at(1);

		dl1 = (vPoint - data.splinePoints.at(0)).magnitude();
		dl2 = (data.splinePoints.at(2) - vPoint).magnitude();
		dt = dl1/(dl1 + dl2/2.0);

		vTan = GetQuadDir(vStart, vControl, vEnd, dt);
		if(vTan.valid)
		{
			spd.splinePoints.append(RS_Vector(vPoint.x - dDist*vTan.y,
				vPoint.y + dDist*vTan.x));
		}

		for(int i = 1; i < n - 1; i++)
		{
			vPoint = data.splinePoints.at(i + 1);

			dl1 = dl2;
			dl2 = (data.splinePoints.at(i + 2) - vPoint).magnitude();
			dt = dl1/(dl1 + dl2);

			vStart = vEnd;
			vControl = data.controlPoints.at(i);
			vEnd = (data.controlPoints.at(i) + data.controlPoints.at(i + 1))/2.0;

			vTan = GetQuadDir(vStart, vControl, vEnd, dt);
			if(vTan.valid)
			{
				spd.splinePoints.append(RS_Vector(vPoint.x - dDist*vTan.y,
					vPoint.y + dDist*vTan.x));
			}
		}

		vPoint = data.splinePoints.at(n);

		dl1 = dl2;
		dl2 = (vPoint - data.splinePoints.at(n + 1)).magnitude();
		dt = dl1/(dl1 + 2.0*dl2);

		vStart = vEnd;
		vControl = data.controlPoints.at(n - 1);
		vEnd = data.splinePoints.at(iPoints - 1);

		vTan = GetQuadDir(vStart, vControl, vEnd, dt);
		if(vTan.valid)
		{
			spd.splinePoints.append(RS_Vector(vPoint.x - dDist*vTan.y,
				vPoint.y + dDist*vTan.x));
		}

		vPoint = data.splinePoints.at(n + 1);

		vTan = GetQuadDir(vStart, vControl, vEnd, 1.0);
		if(vTan.valid)
		{
			spd.splinePoints.append(RS_Vector(vPoint.x - dDist*vTan.y,
				vPoint.y + dDist*vTan.x));
		}
	}
	data = spd;
	return true;
}

QVector<RS_Entity*> AddLineOffsets(const RS_Vector& vx1,
	const RS_Vector& vx2, const double& distance)
{
    QVector<RS_Entity*> ret(0,NULL);

	double dDist = (vx2 - vx1).magnitude();

	if(dDist < RS_TOLERANCE)
	{
		ret << new RS_Circle(NULL, RS_CircleData(vx1, distance));
		return ret;
	}

	LC_SplinePointsData spd1(false);
	LC_SplinePointsData spd2(false);

	LC_SplinePoints *sp1 = new LC_SplinePoints(NULL, spd1);
	LC_SplinePoints *sp2 = new LC_SplinePoints(NULL, spd2);

	dDist = distance/dDist;

	sp1->addPoint(RS_Vector(vx1.x - dDist*(vx2.y - vx1.y), vx1.y + dDist*(vx2.x - vx1.x)));
	sp2->addPoint(RS_Vector(vx1.x + dDist*(vx2.y - vx1.y), vx1.y - dDist*(vx2.x - vx1.x)));

	sp1->addPoint(RS_Vector(vx2.x - dDist*(vx2.y - vx1.y), vx2.y + dDist*(vx2.x - vx1.x)));
	sp2->addPoint(RS_Vector(vx2.x + dDist*(vx2.y - vx1.y), vx2.y - dDist*(vx2.x - vx1.x)));

	ret << sp1;
	ret << sp2;
	return ret;
}

QVector<RS_Entity*> LC_SplinePoints::offsetTwoSides(const double& distance) const
{
    QVector<RS_Entity*> ret(0,NULL);

	int iPoints = data.splinePoints.count();

	if(iPoints < 1) return ret;
	if(iPoints < 2)
	{
		ret << new RS_Circle(NULL, RS_CircleData(data.splinePoints.at(0), distance));
		return ret;
	}

	LC_SplinePointsData spd1(data.closed);
	LC_SplinePointsData spd2(data.closed);

	LC_SplinePoints *sp1, *sp2;

	RS_Vector vStart(false), vEnd(false), vControl(false);
	RS_Vector vPoint(false), vTan(false);

	if(iPoints < 3)
	{
		vStart = data.splinePoints.at(0);
		vEnd = data.splinePoints.at(1);
		ret = AddLineOffsets(vStart, vEnd, distance);
		return ret;
	}

	int n = data.controlPoints.count();

	double dt, dl1, dl2;

	if(data.closed)
	{
		sp1 = new LC_SplinePoints(NULL, spd1);
		sp2 = new LC_SplinePoints(NULL, spd2);

		vPoint = data.splinePoints.at(0);

		dl1 = (data.splinePoints.at(iPoints - 1) - vPoint).magnitude();
		dl2 = (data.splinePoints.at(1) - vPoint).magnitude();
		dt = dl1/(dl1 + dl2);

		vStart = (data.controlPoints.at(n - 1) + data.controlPoints.at(0))/2.0;
		vControl = data.controlPoints.at(0);
		vEnd = (data.controlPoints.at(0) + data.controlPoints.at(1))/2.0;

		vTan = GetQuadDir(vStart, vControl, vEnd, dt);
		if(vTan.valid)
		{
			sp1->addPoint(RS_Vector(vPoint.x - distance*vTan.y,
				vPoint.y + distance*vTan.x));
			sp2->addPoint(RS_Vector(vPoint.x + distance*vTan.y,
				vPoint.y - distance*vTan.x));
		}

		for(int i = 1; i < n - 1; i++)
		{
			vPoint = data.splinePoints.at(i);

			dl1 = dl2;
			dl2 = (data.splinePoints.at(i + 1) - vPoint).magnitude();
			dt = dl1/(dl1 + dl2);

			vStart = (data.controlPoints.at(i - 1) + data.controlPoints.at(i))/2.0;
			vControl = data.controlPoints.at(i);
			vEnd = (data.controlPoints.at(i) + data.controlPoints.at(i + 1))/2.0;

			vTan = GetQuadDir(vStart, vControl, vEnd, dt);

			if(vTan.valid)
			{
				sp1->addPoint(RS_Vector(vPoint.x - distance*vTan.y,
					vPoint.y + distance*vTan.x));
				sp2->addPoint(RS_Vector(vPoint.x + distance*vTan.y,
					vPoint.y - distance*vTan.x));
			}
		}

		vPoint = data.splinePoints.at(iPoints - 1);
		dl1 = (vPoint - data.splinePoints.at(iPoints - 2)).magnitude();
		dl2 = (vPoint - data.splinePoints.at(0)).magnitude();
		dt = dl1/(dl1 + dl2);

		vTan = GetQuadDir(vStart, vControl, vEnd, dt);
		if(vTan.valid)
		{
			sp1->addPoint(RS_Vector(vPoint.x - distance*vTan.y,
				vPoint.y + distance*vTan.x));
			sp2->addPoint(RS_Vector(vPoint.x + distance*vTan.y,
				vPoint.y - distance*vTan.x));
		}
	}
	else
	{
		if(iPoints < 4)
		{
			vStart = data.splinePoints.at(0);
			vTan = data.splinePoints.at(1);
			vEnd = data.splinePoints.at(2);
			dl1 = (vTan - vStart).magnitude();
			dl2 = (vEnd - vTan).magnitude();
			dt = dl1/(dl1 + dl2);

			if(dt < RS_TOLERANCE || dt > 1.0 - RS_TOLERANCE)
			{
				ret = AddLineOffsets(vStart, vEnd, distance);
				return ret;
			}

			vControl = (vTan - vStart*(1.0 - dt)*(1.0 - dt) - vEnd*dt*dt)/dt/(1 - dt)/2.0;

			sp1 = new LC_SplinePoints(NULL, spd1);
			sp2 = new LC_SplinePoints(NULL, spd2);

			vTan = GetQuadDir(vStart, vControl, vEnd, 0.0);
			if(vTan.valid)
			{
				sp1->addPoint(RS_Vector(vPoint.x - distance*vTan.y,
					vPoint.y + distance*vTan.x));
				sp2->addPoint(RS_Vector(vPoint.x + distance*vTan.y,
					vPoint.y - distance*vTan.x));
			}

			vTan = GetQuadDir(vStart, vControl, vEnd, dt);
			if(vTan.valid)
			{
				sp1->addPoint(RS_Vector(vPoint.x - distance*vTan.y,
					vPoint.y + distance*vTan.x));
				sp2->addPoint(RS_Vector(vPoint.x + distance*vTan.y,
					vPoint.y - distance*vTan.x));
			}

			vTan = GetQuadDir(vStart, vControl, vEnd, 1.0);
			if(vTan.valid)
			{
				sp1->addPoint(RS_Vector(vPoint.x - distance*vTan.y,
					vPoint.y + distance*vTan.x));
				sp2->addPoint(RS_Vector(vPoint.x + distance*vTan.y,
					vPoint.y - distance*vTan.x));
			}

			ret << sp1;
			ret << sp2;
			return ret;
		}

		sp1 = new LC_SplinePoints(NULL, spd1);
		sp2 = new LC_SplinePoints(NULL, spd2);

		vPoint = data.splinePoints.at(0);

		vStart = data.splinePoints.at(0);
		vControl = data.controlPoints.at(0);
		vEnd = (data.controlPoints.at(0) + data.controlPoints.at(1))/2.0;

		vTan = GetQuadDir(vStart, vControl, vEnd, 0.0);
		if(vTan.valid)
		{
			sp1->addPoint(RS_Vector(vPoint.x - distance*vTan.y,
				vPoint.y + distance*vTan.x));
			sp2->addPoint(RS_Vector(vPoint.x + distance*vTan.y,
				vPoint.y - distance*vTan.x));
		}

		vPoint = data.splinePoints.at(1);

		dl1 = (vPoint - data.splinePoints.at(0)).magnitude();
		dl2 = (data.splinePoints.at(2) - vPoint).magnitude();
		dt = dl1/(dl1 + dl2/2.0);

		vTan = GetQuadDir(vStart, vControl, vEnd, dt);
		if(vTan.valid)
		{
			sp1->addPoint(RS_Vector(vPoint.x - distance*vTan.y,
				vPoint.y + distance*vTan.x));
			sp2->addPoint(RS_Vector(vPoint.x + distance*vTan.y,
				vPoint.y - distance*vTan.x));
		}

		for(int i = 1; i < n - 1; i++)
		{
			vPoint = data.splinePoints.at(i + 1);

			dl1 = dl2;
			dl2 = (data.splinePoints.at(i + 2) - vPoint).magnitude();
			dt = dl1/(dl1 + dl2);

			vStart = vEnd;
			vControl = data.controlPoints.at(i);
			vEnd = (data.controlPoints.at(i) + data.controlPoints.at(i + 1))/2.0;

			vTan = GetQuadDir(vStart, vControl, vEnd, dt);
			if(vTan.valid)
			{
				sp1->addPoint(RS_Vector(vPoint.x - distance*vTan.y,
					vPoint.y + distance*vTan.x));
				sp2->addPoint(RS_Vector(vPoint.x + distance*vTan.y,
					vPoint.y - distance*vTan.x));
			}
		}

		vPoint = data.splinePoints.at(n);

		dl1 = dl2;
		dl2 = (vPoint - data.splinePoints.at(n + 1)).magnitude();
		dt = dl1/(dl1 + 2.0*dl2);

		vStart = vEnd;
		vControl = data.controlPoints.at(n - 1);
		vEnd = data.splinePoints.at(iPoints - 1);

		vTan = GetQuadDir(vStart, vControl, vEnd, dt);
		if(vTan.valid)
		{
			sp1->addPoint(RS_Vector(vPoint.x - distance*vTan.y,
				vPoint.y + distance*vTan.x));
			sp2->addPoint(RS_Vector(vPoint.x + distance*vTan.y,
				vPoint.y - distance*vTan.x));
		}

		vPoint = data.splinePoints.at(n + 1);

		vTan = GetQuadDir(vStart, vControl, vEnd, 1.0);
		if(vTan.valid)
		{
			sp1->addPoint(RS_Vector(vPoint.x - distance*vTan.y,
				vPoint.y + distance*vTan.x));
			sp2->addPoint(RS_Vector(vPoint.x + distance*vTan.y,
				vPoint.y - distance*vTan.x));
		}
	}

    ret << sp1;
    ret << sp2;
    return ret;
}

/**
 * Dumps the spline's data to stdout.
 */
std::ostream& operator << (std::ostream& os, const LC_SplinePoints& l)
{
	os << " SplinePoints: " << l.getData() << "\n";
	return os;
}

RS_VectorSolutions getLineLineIntersect(RS_Line* l1, const RS_Vector& vx1,
	const RS_Vector& vx2)
{
	RS_VectorSolutions ret;

	RS_Vector x1 = vx2 - vx1;
	RS_Vector x2 = l1->getStartpoint() - l1->getEndpoint();
	RS_Vector x3 = l1->getStartpoint() - vx1;
	
	double dDet = x1.x*x2.y - x1.y*x2.x;
	if(fabs(dDet) < RS_TOLERANCE) return ret;

	double dt = (x2.y*x3.x - x2.x*x3.y)/dDet;
	double ds = (-x1.y*x3.x + x1.x*x3.y)/dDet;

	if(dt < -RS_TOLERANCE) return ret;
	if(ds < -RS_TOLERANCE) return ret;
	if(dt > 1.0 + RS_TOLERANCE) return ret;
	if(ds > 1.0 + RS_TOLERANCE) return ret;
	
	if(dt < 0.0) dt = 0.0;
	if(dt > 1.0) dt = 1.0;

	x3 = vx1*(1.0 - dt) + vx2*dt;

	ret.push_back(x3);

	return ret;
}

void addLineQuadIntersect(RS_VectorSolutions *pVS, RS_Line* l1,
	const RS_Vector& vx1, const RS_Vector& vc1, const RS_Vector& vx2)
{
	RS_Vector x1 = vx2 - vc1*2.0 + vx1;
	RS_Vector x2 = vc1 - vx1;
	RS_Vector x3 = vx1 - l1->getStartpoint();
	RS_Vector x4 = l1->getEndpoint() - l1->getStartpoint();

	double a1 = x1.x*x4.y - x1.y*x4.x;
	double a2 = 2.0*(x2.x*x4.y - x2.y*x4.x);
	double a3 = x3.x*x4.y - x3.y*x4.x;

	std::vector<double> dSol(0, 0.);

	if(fabs(a1) > RS_TOLERANCE)
	{
		std::vector<double> dCoefs(0, 0.);

		dCoefs.push_back(a2/a1);
		dCoefs.push_back(a3/a1);
		dSol = RS_Math::quadraticSolver(dCoefs);

	}
	else if(fabs(a2) > RS_TOLERANCE)
	{
		dSol.push_back(-a3/a2);
	}

	double ds;

    for(double& d: dSol)
	{
        if(d > -RS_TOLERANCE && d < 1.0 + RS_TOLERANCE)
		{
            if(d < 0.0) d = 0.0;
            if(d > 1.0) d = 1.0;

			ds = -1.0;
			x1 = GetQuadAtPoint(vx1, vc1, vx2, d);
			if(fabs(x4.x) > RS_TOLERANCE) ds = (x1.x - l1->getStartpoint().x)/x4.x;
			else if(fabs(x4.y) > RS_TOLERANCE) ds = (x1.y - l1->getStartpoint().y)/x4.y;

			if(ds > -RS_TOLERANCE && ds < 1.0 + RS_TOLERANCE) pVS->push_back(x1);
		}
	}
}

RS_VectorSolutions LC_SplinePoints::getLineIntersect(RS_Line* l1)
{
	RS_VectorSolutions ret;

	int iPoints = data.splinePoints.count();

	if(iPoints < 2) return ret;

	RS_Vector vStart(false), vEnd(false), vControl(false);
	RS_Vector vPoint(false);

	if(iPoints < 3)
	{
		vStart = data.splinePoints.at(0);
		vEnd = data.splinePoints.at(1);
		ret = getLineLineIntersect(l1, vStart, vEnd);
		return ret;
	}

	int n = data.controlPoints.count();

	if(data.closed)
	{
		vStart = (data.controlPoints.at(n - 1) + data.controlPoints.at(0))/2.0;
		vControl = data.controlPoints.at(0);
		vEnd = (data.controlPoints.at(0) + data.controlPoints.at(1))/2.0;

		addLineQuadIntersect(&ret, l1, vStart, vControl, vEnd);

		for(int i = 1; i < n - 1; i++)
		{
			vStart = vEnd;
			vControl = data.controlPoints.at(i);
			vEnd = (data.controlPoints.at(i) + data.controlPoints.at(i + 1))/2.0;

			addLineQuadIntersect(&ret, l1, vStart, vControl, vEnd);
		}

		vStart = vEnd;
		vControl = data.controlPoints.at(n - 1);
		vEnd = (data.controlPoints.at(n - 1) + data.controlPoints.at(0))/2.0;

		addLineQuadIntersect(&ret, l1, vStart, vControl, vEnd);
	}
	else
	{
		if(iPoints < 4)
		{
			vStart = data.splinePoints.at(0);
			vPoint = data.splinePoints.at(1);
			vEnd = data.splinePoints.at(2);
			vControl = GetThreePointsControl(vStart, vPoint, vEnd);

			if(vControl.valid)
				addLineQuadIntersect(&ret, l1, vStart, vControl, vEnd);
			else ret = getLineLineIntersect(l1, vStart, vEnd);
			return ret;
		}

		vStart = data.splinePoints.at(0);
		vControl = data.controlPoints.at(0);
		vEnd = (data.controlPoints.at(0) + data.controlPoints.at(1))/2.0;

		addLineQuadIntersect(&ret, l1, vStart, vControl, vEnd);

		for(int i = 1; i < n - 1; i++)
		{
			vStart = vEnd;
			vControl = data.controlPoints.at(i);
			vEnd = (data.controlPoints.at(i) + data.controlPoints.at(i + 1))/2.0;

			addLineQuadIntersect(&ret, l1, vStart, vControl, vEnd);
		}

		vStart = vEnd;
		vControl = data.controlPoints.at(n - 1);
		vEnd = data.splinePoints.at(iPoints - 1);

		addLineQuadIntersect(&ret, l1, vStart, vControl, vEnd);
	}

    return ret;
}

RS_VectorSolutions LC_SplinePoints::getSplinePointsIntersect(LC_SplinePoints* l1)
{
	RS_VectorSolutions ret;
    return ret;
}

RS_VectorSolutions getQuadraticLineIntersect(std::vector<double> dQuadCoefs,
	const RS_Vector& vx1, const RS_Vector& vx2)
{
	RS_VectorSolutions ret;
	if(dQuadCoefs.size() < 3) return ret;

	RS_Vector x1 = vx2 - vx1;

	double a0 = 0.0;
	double a1 = 0.0;
	double a2 = 0.0;

	if(dQuadCoefs.size() > 3)
	{
		a2 = dQuadCoefs[0]*x1.x*x1.x + dQuadCoefs[1]*x1.x*x1.y + dQuadCoefs[2]*x1.y*x1.y;
		a1 = 2.0*(dQuadCoefs[0]*x1.x*vx1.x + dQuadCoefs[2]*x1.y*vx1.y) +
			dQuadCoefs[1]*(x1.x*vx1.y + x1.y*vx1.x) + dQuadCoefs[3]*x1.x + dQuadCoefs[4]*x1.y;
		a0 = dQuadCoefs[0]*vx1.x*vx1.x + dQuadCoefs[1]*vx1.x*vx1.y + dQuadCoefs[2]*vx1.y*vx1.y +
			dQuadCoefs[3]*vx1.x + dQuadCoefs[4]*vx1.y + dQuadCoefs[5];
	}
	else
	{
		a1 = dQuadCoefs[0]*x1.x + dQuadCoefs[1]*x1.y;
		a0 = dQuadCoefs[0]*vx1.x + dQuadCoefs[1]*vx1.y + dQuadCoefs[2];
	}

	std::vector<double> dSol(0, 0.);
	std::vector<double> dCoefs(0, 0.);

	if(fabs(a2) > RS_TOLERANCE)
	{
		dCoefs.push_back(a1/a2);
		dCoefs.push_back(a0/a2);
		dSol = RS_Math::quadraticSolver(dCoefs);

	}
	else if(fabs(a1) > RS_TOLERANCE)
	{
		dSol.push_back(-a0/a1);
	}

    for(double& d: dSol)
	{
        if(d > -RS_TOLERANCE && d < 1.0 + RS_TOLERANCE)
		{
            if(d < 0.0) d = 0.0;
            if(d > 1.0) d = 1.0;
			ret.push_back(vx1*(1.0 - d) + vx2*d);
		}
	}

	return ret;
}

void addQuadraticQuadIntersect(RS_VectorSolutions *pVS, std::vector<double> dQuadCoefs,
	const RS_Vector& vx1, const RS_Vector& vc1, const RS_Vector& vx2)
{
	if(dQuadCoefs.size() < 3) return;

	RS_Vector x1 = vx2 - vc1*2.0 + vx1;
	RS_Vector x2 = vc1 - vx1;

	double a0 = 0.0;
	double a1 = 0.0;
	double a2 = 0.0;
	double a3 = 0.0;
	double a4 = 0.0;

	if(dQuadCoefs.size() > 3)
	{
		a4 = dQuadCoefs[0]*x1.x*x1.x + dQuadCoefs[1]*x1.x*x1.y + dQuadCoefs[2]*x1.y*x1.y;
		a3 = 4.0*dQuadCoefs[0]*x1.x*x2.x + 2.0*dQuadCoefs[1]*(x1.x*x2.y + x1.y*x2.x) +
			4.0*dQuadCoefs[2]*x1.y*x2.y;
		a2 = dQuadCoefs[0]*(2.0*x1.x*vx1.x + 4.0*x2.x*x2.x) +
			dQuadCoefs[1]*(x1.x*vx1.y + x1.y*vx1.x + 4.0*x2.x*x2.y) +
			dQuadCoefs[2]*(2.0*x1.y*vx1.y + 4.0*x2.y*x2.y) +
			dQuadCoefs[3]*x1.x + dQuadCoefs[4]*x1.y;
		a1 = 4.0*(dQuadCoefs[0]*x2.x*vx1.x + dQuadCoefs[2]*x2.y*vx1.y) +
			2.0*(dQuadCoefs[1]*(x2.x*vx1.y + x2.y*vx1.x) + dQuadCoefs[3]*x2.x + dQuadCoefs[4]*x2.y);
		a0 = dQuadCoefs[0]*vx1.x*vx1.x + dQuadCoefs[1]*vx1.x*vx1.y + dQuadCoefs[2]*vx1.y*vx1.y +
			dQuadCoefs[3]*vx1.x + dQuadCoefs[4]*vx1.y + dQuadCoefs[5];
	}
	else
	{
		a2 = dQuadCoefs[0]*x1.x + dQuadCoefs[1]*x1.y;
		a1 = 2.0*(dQuadCoefs[0]*x2.x + dQuadCoefs[1]*x2.y);
		a0 = dQuadCoefs[0]*vx1.x + dQuadCoefs[1]*vx1.y + dQuadCoefs[2];
	}

	std::vector<double> dSol(0, 0.);
	std::vector<double> dCoefs(0, 0.);

	if(fabs(a4) > RS_TOLERANCE)
	{
		dCoefs.push_back(a3/a4);
		dCoefs.push_back(a2/a4);
		dCoefs.push_back(a1/a4);
		dCoefs.push_back(a0/a4);
		dSol = RS_Math::quarticSolver(dCoefs);
	}
	else if(fabs(a3) > RS_TOLERANCE)
	{
		dCoefs.push_back(a2/a3);
		dCoefs.push_back(a1/a3);
		dCoefs.push_back(a0/a3);
		dSol = RS_Math::cubicSolver(dCoefs);
	}
	else if(fabs(a2) > RS_TOLERANCE)
	{
		dCoefs.push_back(a1/a2);
		dCoefs.push_back(a0/a2);
		dSol = RS_Math::quadraticSolver(dCoefs);

	}
	else if(fabs(a1) > RS_TOLERANCE)
	{
		dSol.push_back(-a0/a1);
	}

    for(double& d: dSol)
	{
        if(d > -RS_TOLERANCE && d < 1.0 + RS_TOLERANCE)
		{
            if(d < 0.0) d = 0.0;
            if(d > 1.0) d = 1.0;
			pVS->push_back(GetQuadAtPoint(vx1, vc1, vx2, d));
		}
	}
}

RS_VectorSolutions LC_SplinePoints::getQuadraticIntersect(RS_Entity* e1)
{
	RS_VectorSolutions ret;

	LC_Quadratic lcQuad = e1->getQuadratic();
	std::vector<double> dQuadCoefs = lcQuad.getCoefficients();

	int iPoints = data.splinePoints.count();

	if(iPoints < 2) return ret;

	RS_Vector vStart(false), vEnd(false), vControl(false);
	RS_Vector vPoint(false);

	if(iPoints < 3)
	{
		vStart = data.splinePoints.at(0);
		vEnd = data.splinePoints.at(1);
		ret = getQuadraticLineIntersect(dQuadCoefs, vStart, vEnd);
		return ret;
	}

	int n = data.controlPoints.count();

	if(data.closed)
	{
		vStart = (data.controlPoints.at(n - 1) + data.controlPoints.at(0))/2.0;
		vControl = data.controlPoints.at(0);
		vEnd = (data.controlPoints.at(0) + data.controlPoints.at(1))/2.0;

		addQuadraticQuadIntersect(&ret, dQuadCoefs, vStart, vControl, vEnd);

		for(int i = 1; i < n - 1; i++)
		{
			vStart = vEnd;
			vControl = data.controlPoints.at(i);
			vEnd = (data.controlPoints.at(i) + data.controlPoints.at(i + 1))/2.0;

			addQuadraticQuadIntersect(&ret, dQuadCoefs, vStart, vControl, vEnd);
		}

		vStart = vEnd;
		vControl = data.controlPoints.at(n - 1);
		vEnd = (data.controlPoints.at(n - 1) + data.controlPoints.at(0))/2.0;

		addQuadraticQuadIntersect(&ret, dQuadCoefs, vStart, vControl, vEnd);
	}
	else
	{
		if(iPoints < 4)
		{
			vStart = data.splinePoints.at(0);
			vPoint = data.splinePoints.at(1);
			vEnd = data.splinePoints.at(2);
			vControl = GetThreePointsControl(vStart, vPoint, vEnd);

			if(vControl.valid)
				addQuadraticQuadIntersect(&ret, dQuadCoefs, vStart, vControl, vEnd);
			else ret = getQuadraticLineIntersect(dQuadCoefs, vStart, vEnd);
			return ret;
		}

		vStart = data.splinePoints.at(0);
		vControl = data.controlPoints.at(0);
		vEnd = (data.controlPoints.at(0) + data.controlPoints.at(1))/2.0;

		addQuadraticQuadIntersect(&ret, dQuadCoefs, vStart, vControl, vEnd);

		for(int i = 1; i < n - 1; i++)
		{
			vStart = vEnd;
			vControl = data.controlPoints.at(i);
			vEnd = (data.controlPoints.at(i) + data.controlPoints.at(i + 1))/2.0;

			addQuadraticQuadIntersect(&ret, dQuadCoefs, vStart, vControl, vEnd);
		}

		vStart = vEnd;
		vControl = data.controlPoints.at(n - 1);
		vEnd = data.splinePoints.at(iPoints - 1);

		addQuadraticQuadIntersect(&ret, dQuadCoefs, vStart, vControl, vEnd);
	}

    return ret;
}


RS_VectorSolutions LC_SplinePoints::getIntersection(RS_Entity* e1, RS_Entity* e2)
{
	RS_VectorSolutions ret;

	if(e1->rtti() != RS2::EntitySplinePoints) //std::swap(&e1, &e2);
	{
		RS_Entity *etmp = e2;
		e2 = e1;
		e1 = etmp;
	}
	if(e1->rtti() != RS2::EntitySplinePoints) return ret;
	
	switch(e2->rtti())
	{
	case RS2::EntityLine:
		ret = ((LC_SplinePoints*)e1)->getLineIntersect((RS_Line*)e2);
		break;
	case RS2::EntitySplinePoints:
		ret = ((LC_SplinePoints*)e1)->getSplinePointsIntersect((LC_SplinePoints*)e2);
		break;
	default:
		ret = ((LC_SplinePoints*)e1)->getQuadraticIntersect(e2);
	}

	return ret;
}

