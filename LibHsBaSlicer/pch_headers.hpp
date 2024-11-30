#include <vector>
#include <list>
#include <unordered_map>
#include <map>
#include <set>
#include <unordered_set>
#include <any>
#include <variant>
#include <string_view>
#include <string>
#include <algorithm>
#include <regex>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <coroutine>
#include <memory>

#include <boost/any.hpp>
#include <boost/variant.hpp>
#include <boost/variant2.hpp>
#include <boost/date_time.hpp>

#include <Eigen/Core>

#include "base/concepts.hpp"
#include "base/error.hpp"
#include "base/template_helper.hpp"
#include "base/encoding_convert.hpp"

#include "meshmodel/IglModel.hpp"
#include "meshmodel/CgalModel.hpp"
#include "cadmodel/OcctModel.hpp"
#include "meshmodel/FullTopoModel.hpp"

#include "2D/IntPolygon.hpp"