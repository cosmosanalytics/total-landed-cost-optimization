cmake_minimum_required(VERSION 3.16)
project(total_landed_cost CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(USE_CBC "Build the CbcMipSolver backend (requires COIN-OR CBC dev libraries)" OFF)

include_directories(include)

set(CORE_SOURCES
    src/ShippingCostModel.cpp
    src/LandedCostProblem.cpp
    src/GreedyLowestCostSolver.cpp
    src/BranchAndBoundSolver.cpp
)

add_library(landedcost_core ${CORE_SOURCES})
target_include_directories(landedcost_core PUBLIC include)

if(USE_CBC)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(CBC REQUIRED cbc)
  target_compile_definitions(landedcost_core PUBLIC LANDEDCOST_USE_CBC)
  target_include_directories(landedcost_core PUBLIC ${CBC_INCLUDE_DIRS})
  target_link_libraries(landedcost_core PUBLIC ${CBC_LIBRARIES})
endif()

add_executable(landedcost_demo src/main.cpp)
target_link_libraries(landedcost_demo PRIVATE landedcost_core)

add_executable(landedcost_tests
    tests/main.cpp
    tests/LandedCostTests.cpp
)
target_include_directories(landedcost_tests PRIVATE tests)
target_link_libraries(landedcost_tests PRIVATE landedcost_core)

enable_testing()
add_test(NAME unit_tests COMMAND landedcost_tests)
