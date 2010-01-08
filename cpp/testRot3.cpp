/**
 * @file    testRot3.cpp
 * @brief   Unit tests for Rot3 class
 * @author  Alireza Fathi
 */

#include <CppUnitLite/TestHarness.h>
#include "numericalDerivative.h"
#include "Point3.h"
#include "Rot3.h"

using namespace gtsam;

Rot3 R = rodriguez(0.1,0.4,0.2);
Point3 P(0.2,0.7,-2.0);
double error = 1e-9, epsilon=0.001;

/* ************************************************************************* */
TEST( Rot3, constructor) {
  Rot3 expected(eye(3,3));
  Vector r1(3), r2(3), r3(3);
  r1(0)=1;r1(1)=0;r1(2)=0;
  r2(0)=0;r2(1)=1;r2(2)=0;
  r3(0)=0;r3(1)=0;r3(2)=1;
  Rot3 actual(r1,r2,r3);
  CHECK(assert_equal(actual,expected));
}

/* ************************************************************************* */
TEST( Rot3, constructor2) {
  Matrix R = Matrix_(3,3,
		       11.,12.,13.,
		       21.,22.,23.,
		       31.,32.,33.);
  Rot3 actual(R);
  Rot3 expected(11,12,13,
		21,22,23,
		31,32,33);
  CHECK(assert_equal(actual,expected));
}

/* ************************************************************************* */
TEST( Rot3, constructor3) {
  Rot3 expected(1,2,3,4,5,6,7,8,9);
  Point3 r1(1,4,7), r2(2,5,8), r3(3,6,9);
  CHECK(assert_equal(Rot3(r1,r2,r3),expected));
}

/* ************************************************************************* */
TEST( Rot3, transpose) {
  Rot3 R(1,2,3,4,5,6,7,8,9);
  Point3 r1(1,2,3), r2(4,5,6), r3(7,8,9);
  CHECK(assert_equal(inverse(R),Rot3(r1,r2,r3)));
}

/* ************************************************************************* */
TEST( Rot3, equals) {
  CHECK(R.equals(R));
  Rot3 zero;
  CHECK(!R.equals(zero));
}

/* ************************************************************************* */
Rot3 slow_but_correct_rodriguez(const Vector& w) {
	double t = norm_2(w);
	Matrix J = skewSymmetric(w/t);
	if (t < 1e-5) return Rot3();
	Matrix R = eye(3, 3) + sin(t) * J + (1.0 - cos(t)) * (J * J);
	return R; // matrix constructor will be tripped
}

/* ************************************************************************* */
TEST( Rot3, rodriguez) {
  Rot3 R1 = rodriguez(epsilon, 0, 0);
  Vector w = Vector_(3,epsilon,0.,0.);
  Rot3 R2 = slow_but_correct_rodriguez(w);
  CHECK(assert_equal(R1,R2));
}

/* ************************************************************************* */
TEST( Rot3, rodriguez2) {
  Vector v(3); v(0) = 0; v(1) = 1; v(2) = 0;
  Rot3 R1 = rodriguez(v, 3.14/4.0);
  Rot3 R2(0.707388,0,0.706825,
  		0,1,0,
  		-0.706825,0,0.707388);
  CHECK(assert_equal(R1,R2,1e-5));
}

/* ************************************************************************* */
TEST( Rot3, rodriguez3) {
  Vector w = Vector_(3,0.1,0.2,0.3);
  Rot3 R1 = rodriguez(w/norm_2(w), norm_2(w));
  Rot3 R2 = slow_but_correct_rodriguez(w);
  CHECK(assert_equal(R1,R2));
}

/* ************************************************************************* */
TEST( Rot3, expmap)
{
  Vector v(3);
  fill(v.begin(), v.end(), 0);
  CHECK(assert_equal(expmap(R,v), R));
}

/* ************************************************************************* */
// rotate derivatives

TEST( Rot3, Drotate1)
{
  Matrix computed = Drotate1(R, P);
  Matrix numerical = numericalDerivative21(rotate,R,P);
  CHECK(assert_equal(numerical,computed,error));
}

TEST( Rot3, Drotate1_)
  {
  Matrix computed = Drotate1(inverse(R), P);
  Matrix numerical = numericalDerivative21(rotate,inverse(R),P);
  CHECK(assert_equal(numerical,computed,error));
}

TEST( Rot3, Drotate2_DNrotate2)
{
  Matrix computed = Drotate2(R);
  Matrix numerical = numericalDerivative22(rotate,R,P);
  CHECK(assert_equal(numerical,computed,error));
}

/* ************************************************************************* */
// unrotate 

TEST( Rot3, unrotate)
{
  Point3 w = R*P;
  CHECK(assert_equal(unrotate(R,w),P));
}

/* ************************************************************************* */
// unrotate derivatives

TEST( Rot3, Dunrotate1)
{
  Matrix computed = Dunrotate1(R, P);
  Matrix numerical = numericalDerivative21(unrotate,R,P);
  CHECK(assert_equal(numerical,computed,error));
}

TEST( Rot3, Dunrotate2_DNunrotate2)
{
  Matrix computed = Dunrotate2(R);
  Matrix numerical = numericalDerivative22(unrotate,R,P);
  CHECK(assert_equal(numerical,computed,error));
}

/* ************************************************************************* */
TEST( Rot3, compose )
{
	Rot3 R1 = rodriguez(0.1, 0.2, 0.3);
	Rot3 R2 = rodriguez(0.2, 0.3, 0.5);

	Rot3 expected = R1*R2;
	Rot3 actual = compose(R1, R2);
	CHECK(assert_equal(expected,actual));

	Matrix numericalH1 = numericalDerivative21<Rot3,Rot3,Rot3>(compose, R1, R2, 1e-5);
	Matrix actualH1 = Dcompose1(R1, R2);
	CHECK(assert_equal(numericalH1,actualH1));

	Matrix actualH2 = Dcompose2(R1, R2);
	Matrix numericalH2 = numericalDerivative22<Rot3,Rot3,Rot3>(compose, R1, R2, 1e-5);
	CHECK(assert_equal(numericalH2,actualH2));
}

/* ************************************************************************* */
TEST( Rot3, between )
{
	Rot3 R1 = rodriguez(0.1, 0.2, 0.3);
	Rot3 R2 = rodriguez(0.2, 0.3, 0.5);

	Rot3 expected = R2*inverse(R1);
	Rot3 actual = between(R1, R2);
	CHECK(assert_equal(expected,actual));

	Matrix numericalH1 = numericalDerivative21(between<Rot3> , R1, R2, 1e-5);
	Matrix actualH1 = Dbetween1(R1, R2);
	CHECK(assert_equal(numericalH1,actualH1));

	Matrix actualH2 = Dbetween2(R1, R2);
	Matrix numericalH2 = numericalDerivative22(between<Rot3> , R1, R2, 1e-5);
	CHECK(assert_equal(numericalH2,actualH2));
}

/* ************************************************************************* */
int main(){ TestResult tr; return TestRegistry::runAllTests(tr); }
/* ************************************************************************* */

