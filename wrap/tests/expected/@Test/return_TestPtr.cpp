// automatically generated by wrap
#include <wrap/matlab.h>
#include <folder/path/to/Test.h>
using namespace geometry;
void mexFunction(int nargout, mxArray *out[], int nargin, const mxArray *in[])
{
  checkArguments("return_TestPtr",nargout,nargin-1,1);
  boost::shared_ptr<Test> self = unwrap_shared_ptr< Test >(in[0],"Test");
  boost::shared_ptr<Test> value = unwrap_shared_ptr< Test >(in[1], "Test");
  boost::shared_ptr<Test> result = self->return_TestPtr(value);
  out[0] = wrap_shared_ptr(result,"Test");
}
