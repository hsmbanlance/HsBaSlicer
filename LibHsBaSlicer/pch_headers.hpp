#include <algorithm>
#include <any>
#include <coroutine>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <set>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#include <boost/any.hpp>
#include <boost/date_time.hpp>
#include <boost/variant.hpp>
#include <boost/variant2.hpp>

#include <Eigen/Core>

#include "base/concepts.hpp"
#include "base/encoding_convert.hpp"
#include "base/error.hpp"
#include "base/template_helper.hpp"

#include "meshmodel/IglModel.hpp"
#ifdef USE_CGAL
#include "meshmodel/CgalModel.hpp"
#endif  // USE_CGAL
#ifdef USE_OCCT
#include "cadmodel/OcctModel.hpp"
#endif  // USE_OCCT
#include "meshmodel/FullTopoModel.hpp"

#include "2D/IntPolygon.hpp"