// automatically generated by wrap
#include <wrap/matlab.h>
#include <Point2.h>
void mexFunction(int nargout, mxArray *out[], int nargin, const mxArray *in[])
{
  checkArguments("dim",nargout,nargin-1,0);
  boost::shared_ptr<Point2> self = unwrap_shared_ptr< Point2 >(in[0],"Point2");
  int result = self->dim();
  out[0] = wrap< int >(result);
}
