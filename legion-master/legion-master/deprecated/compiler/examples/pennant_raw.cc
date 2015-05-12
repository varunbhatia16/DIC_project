/* Copyright 2014 Stanford University and Los Alamos National Security, LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "pennant.h"
#include "pennant.lg.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "default_mapper.h"
#include "legion_tasks.h"
#include "mapping_utilities.h"
#include "utilities.h"

///
/// Configuration
///

config::config()
  : alfa(0.5)
  , bcx_0(0.0), bcx_1(0.0), bcy_0(0.0), bcy_1(0.0)
  , bcx_n(0), bcy_n(0)
  , cfl(0.6)
  , cflv(0.1)
  , cstop(999999)
  , tstop(1e99)
  , chunksize(99999999)
  , dtfac(1.2)
  , dtinit(1e99)
  , dtmax(1e99)
  , dtreport(10)
  , einit(0.0)
  , einitsub(0.0)
  , gamma(5.0 / 3.0)
  , meshscale(1.0)
  , q1(0.0)
  , q2(2.0)
  , qgamma(5.0 / 3.0)
  , rinit(1.0)
  , rinitsub(1.0)
  , ssmin(0.0)
  , subregion_0(0.0), subregion_1(0.0), subregion_2(0.0), subregion_3(0.0)
  , uinitradial(0.0)
{}

///
/// Constants
///

typedef AccessorType::AOS<176> AOS_ZONES;
typedef AccessorType::AOS<124> AOS_POINTS;
typedef AccessorType::AOS<244> AOS_SIDES;
typedef AccessorType::AOS<8> AOS_REDUCE;

///
/// Command-line defaults
///

const std::string default_input_filename = "pennant.tests/sedovsmall/sedovsmall.pnt";
const intptr_t default_npieces = 2;
const bool default_use_foreign = true;

///
/// Input helpers
///

static std::string get_input_filename()
{
  const InputArgs &args = HighLevelRuntime::get_input_args();

  std::string filename = default_input_filename;
  for (int i = 1; i < args.argc; i++) {
    std::string arg = args.argv[i];

    // Skip options.
    if (arg.find('-') == 0) {
      i++;
      continue;
    }

    // Grab positional parameter.
    filename = arg;
    break;
  }
  return filename;
}

static std::string get_solution_filename()
{
  std::string input_filename = get_input_filename();

  size_t sep = input_filename.find_last_of('.');
  if (sep != std::string::npos) {
    return input_filename.substr(0, sep) + ".xy.std";
  } else {
    return input_filename + ".xy.std";
  }
}

static std::string get_directory(const std::string &filename)
{
  size_t sep = filename.find_last_of('/');
  if (sep != std::string::npos) {
    return filename.substr(0, sep + 1);
  } else {
    return ".";
  }
}

static intptr_t get_npieces()
{
  const InputArgs &args = HighLevelRuntime::get_input_args();

  for (int i = 1; i < args.argc; i++) {
    std::string arg = args.argv[i];

    // Grab npieces parameter.
    if (arg == "-npieces") {
      i++;
      return std::stoll(args.argv[i]);
    }

    // Skip other options.
    if (arg.find('-') == 0) {
      i++;
      continue;
    }
  }
  return default_npieces;
}

static bool get_use_foreign()
{
  const InputArgs &args = HighLevelRuntime::get_input_args();

  for (int i = 1; i < args.argc; i++) {
    std::string arg = args.argv[i];

    // Grab npieces parameter.
    if (arg == "-foreign") {
      i++;
      return bool(std::stoll(args.argv[i]));
    }

    // Skip other options.
    if (arg.find('-') == 0) {
      i++;
      continue;
    }
  }
  return default_use_foreign;
}

static void extract_param(const std::map<std::string, std::vector<std::string> > &params,
                          const std::string &key, int idx, double &var)
{
  if (params.count(key)) {
    assert(params.at(key).size() > idx);
    var = stod(params.at(key)[idx]);
  }
}

static void extract_param(const std::map<std::string, std::vector<std::string> > params,
                          const std::string &key, int idx, intptr_t &var)
{
  if (params.count(key)) {
    assert(params.at(key).size() > idx);
    var = (intptr_t)stoll(params.at(key)[idx]);
  }
}

static void read_params(const std::string &pnt_filename,
                        std::map<std::string, std::vector<std::string> > &params)
{
  std::ifstream pnt_file(pnt_filename.c_str(), std::ios::in);

  // Abort if file not valid.
  if (!pnt_file) {
    assert(0 && "input file does not exist");
  }

  // Read all lines.
  while (pnt_file) {
    std::string line;
    getline(pnt_file, line);

    std::stringstream split(line, std::ios::in);

    std::string name;
    split >> name;

    // Skip blank lines.
    if (name.size() == 0) {
      continue;
    }

    // Slurp rest of line.
    std::vector<std::string> data;
    while (split) {
      std::string datum;
      split >> datum;
      if (datum.size() > 0) {
        data.push_back(datum);
      }
    }

    params[name] = data;
  }
}

static void read_mesh(const std::string &gmv_filename,
                      config &conf,
                      std::vector<double> &pxx,
                      std::vector<double> &pxy,
                      std::vector<std::vector<intptr_t> > &mapzp)
{
  std::ifstream gmv_file(gmv_filename.c_str(), std::ios::in);

  intptr_t np = 0;
  intptr_t nz = 0;
  intptr_t ns = 0;
  intptr_t maxznump = 0;

  // Abort if file not valid.
  if (!gmv_file) {
    assert(0 && "input file does not exist");
  }

  {
    std::string line;
    getline(gmv_file, line);
    assert(line == "gmvinput ascii");
  }

  {
    std::string token;
    gmv_file >> token;
    assert(token == "nodes");
    gmv_file >> np;
  }

  for (int ip = 0; ip < np; ip++) {
    double x;
    gmv_file >> x;
    pxx.push_back(x);
  }

  for (int ip = 0; ip < np; ip++) {
    double y;
    gmv_file >> y;
    pxy.push_back(y);
  }

  for (int ip = 0; ip < np; ip++) {
    double z;
    gmv_file >> z;
    // Throw away z coordinates.
  }

  {
    std::string token;
    gmv_file >> token;
    assert(token == "cells");
    gmv_file >> nz;
  }

  for (int iz = 0; iz < nz; iz++) {
    {
      std::string token;
      gmv_file >> token;
      assert(token == "general");
      double nf;
      gmv_file >> nf;
      assert(nf == 1);
    }

    double znump;
    gmv_file >> znump;

    if (znump > maxznump) {
      maxznump = znump;
    }
    ns += znump;

    std::vector<intptr_t> points;
    for (int zip = 0; zip < znump; zip++) {
      intptr_t ip;
      gmv_file >> ip;
      points.push_back(ip - 1);
    }
    mapzp.push_back(points);
  }

  conf.nz = nz;
  conf.np = np;
  conf.ns = ns;
  conf.maxznump = maxznump;
}

///
/// I/O
///

config read_config()
{
  std::string pnt_filename = get_input_filename();
  std::string dir = get_directory(pnt_filename);

  printf("Reading %s\n", pnt_filename.c_str());

  // Hack: Read inputs twice (including mesh) because there is no
  // (safe) way to save data between task calls.
  std::map<std::string, std::vector<std::string> > params;
  read_params(pnt_filename, params);

  config conf;
  {
    std::vector<double> pxx;
    std::vector<double> pxy;
    std::vector<std::vector<intptr_t> > mapzp;

    std::string meshfile("meshfile");
    assert(params.count(meshfile));
    assert(params[meshfile].size() == 1);
    std::string gmv_filename = dir + params[meshfile][0];

    read_mesh(gmv_filename, conf, pxx, pxy, mapzp);
  }

  conf.npieces = get_npieces();
  conf.use_foreign = get_use_foreign();

  printf("Using npieces %ld\n", conf.npieces);

  extract_param(params, "cstop", 0, conf.cstop);
  extract_param(params, "tstop", 0, conf.tstop);
  extract_param(params, "meshscale", 0, conf.meshscale);
  extract_param(params, "subregion", 0, conf.subregion_0);
  extract_param(params, "subregion", 1, conf.subregion_1);
  extract_param(params, "subregion", 2, conf.subregion_2);
  extract_param(params, "subregion", 3, conf.subregion_3);
  extract_param(params, "cfl", 0, conf.cfl);
  extract_param(params, "cflv", 0, conf.cflv);
  extract_param(params, "rinit", 0, conf.rinit);
  extract_param(params, "einit", 0, conf.einit);
  extract_param(params, "rinitsub", 0, conf.rinitsub);
  extract_param(params, "einitsub", 0, conf.einitsub);
  extract_param(params, "uinitradial", 0, conf.uinitradial);
  extract_param(params, "bcx", 0, conf.bcx_0);
  extract_param(params, "bcx", 1, conf.bcx_1);
  extract_param(params, "bcy", 0, conf.bcy_0);
  extract_param(params, "bcy", 1, conf.bcy_1);
  conf.bcx_n = params["bcx"].size();
  conf.bcy_n = params["bcy"].size();
  extract_param(params, "ssmin", 0, conf.ssmin);
  extract_param(params, "q1", 0, conf.q1);
  extract_param(params, "q2", 0, conf.q2);
  extract_param(params, "dtinit", 0, conf.dtinit);
  extract_param(params, "chunksize", 0, conf.chunksize);

  return conf;
}

const intptr_t nocolors = -1;
const intptr_t manycolors = -2;
const intptr_t pcolors_bits = 64;

void foreign_read_input(HighLevelRuntime *runtime,
                        Context ctx,
                        config conf,
                        PhysicalRegion rz_all[1],
                        PhysicalRegion rp_all[1],
                        PhysicalRegion rs_all[1],
                        PhysicalRegion rm_all[1],
                        PhysicalRegion pcolor_a[1],
                        PhysicalRegion pcolors_a[1],
                        PhysicalRegion pcolor_shared_a[1])
{
  std::string pnt_filename = get_input_filename();
  std::string dir = get_directory(pnt_filename);

  // Read mesh.
  std::vector<double> px_x;
  std::vector<double> px_y;
  std::vector<std::vector<intptr_t> > mapzp;
  {
    // Hack: Read inputs twice (including mesh) because there is no
    // (safe) way to save data between task calls.
    std::map<std::string, std::vector<std::string> > params;
    read_params(pnt_filename, params);

    std::string meshfile("meshfile");
    assert(params.count(meshfile));
    assert(params[meshfile].size() == 1);
    std::string gmv_filename = dir + params[meshfile][0];

    config conf;

    read_mesh(gmv_filename, conf, px_x, px_y, mapzp);
  }

  // Allocate mesh.
  {
    IndexAllocator rz_all_alloc =
      runtime->create_index_allocator(ctx, rz_all[0].get_logical_region().get_index_space());
    IndexAllocator rp_all_alloc =
      runtime->create_index_allocator(ctx, rp_all[0].get_logical_region().get_index_space());
    IndexAllocator rs_all_alloc =
      runtime->create_index_allocator(ctx, rs_all[0].get_logical_region().get_index_space());
    IndexAllocator rm_all_alloc =
      runtime->create_index_allocator(ctx, rm_all[0].get_logical_region().get_index_space());

    rz_all_alloc.alloc(conf.nz);
    rp_all_alloc.alloc(conf.np);
    rs_all_alloc.alloc(conf.ns);
    rm_all_alloc.alloc(conf.npieces);
  }

  // Initialize zones.
  std::vector<intptr_t> m_zstart(conf.npieces);
  std::vector<intptr_t> m_zend(conf.npieces);
  std::vector<intptr_t> m_sstart(conf.npieces);
  std::vector<intptr_t> m_send(conf.npieces);
  {
    RegionAccessor<AOS_ZONES, intptr_t> accessor_znump =
      rz_all[0].get_field_accessor(FIELD_ZNUMP).typeify<intptr_t>().convert<AOS_ZONES>();

    intptr_t zones_per_color = (conf.nz + conf.npieces - 1) / conf.npieces;

    intptr_t zstart = 0, zend = 0, sstart = 0, send = 0;
    for (intptr_t m = 0; m < conf.npieces; m++) {
      zstart = zend;
      zend = std::min(zend + zones_per_color, conf.nz);

      sstart = send;

      for (intptr_t z = zstart; z < zend; z++) {
        intptr_t znump = mapzp[z].size();
        accessor_znump.write(z, znump);
        send += znump;
      }

      m_zstart[m] = zstart;
      m_zend[m] = zend;

      m_sstart[m] = sstart;
      m_send[m] = send;
    }
  }

  // Initialize points.
  {
    RegionAccessor<AOS_POINTS, double> accessor_px_x =
      rp_all[0].get_field_accessor(FIELD_PX_X).typeify<double>().convert<AOS_POINTS>();
    RegionAccessor<AOS_POINTS, double> accessor_px_y =
      rp_all[0].get_field_accessor(FIELD_PX_Y).typeify<double>().convert<AOS_POINTS>();
    RegionAccessor<AOS_POINTS, bool> accessor_has_bcx_0 =
      rp_all[0].get_field_accessor(FIELD_HAS_BCX_0).typeify<bool>().convert<AOS_POINTS>();
    RegionAccessor<AOS_POINTS, bool> accessor_has_bcx_1 =
      rp_all[0].get_field_accessor(FIELD_HAS_BCX_1).typeify<bool>().convert<AOS_POINTS>();
    RegionAccessor<AOS_POINTS, bool> accessor_has_bcy_0 =
      rp_all[0].get_field_accessor(FIELD_HAS_BCY_0).typeify<bool>().convert<AOS_POINTS>();
    RegionAccessor<AOS_POINTS, bool> accessor_has_bcy_1 =
      rp_all[0].get_field_accessor(FIELD_HAS_BCY_1).typeify<bool>().convert<AOS_POINTS>();

    RegionAccessor<AccessorType::AOS<0>, intptr_t> accessor_pcolor =
      pcolor_a[0].get_accessor().typeify<intptr_t>().convert<AccessorType::AOS<0> >();

    RegionAccessor<AccessorType::AOS<0>, uint64_t> accessor_pcolors =
      pcolors_a[0].get_accessor().typeify<uint64_t>().convert<AccessorType::AOS<0> >();

    RegionAccessor<AccessorType::AOS<0>, intptr_t> accessor_pcolor_shared =
      pcolor_shared_a[0].get_accessor().typeify<intptr_t>().convert<AccessorType::AOS<0> >();

    const double eps = 1e-12;
    for (intptr_t p = 0; p < conf.np; p++) {
      accessor_px_x.write(p, px_x[p] * conf.meshscale);
      accessor_px_y.write(p, px_y[p] * conf.meshscale);

      accessor_has_bcx_0.write(p, conf.bcx_n > 0 && fabs(px_x[p] - conf.bcx_0) < eps);
      accessor_has_bcx_1.write(p, conf.bcx_n > 1 && fabs(px_x[p] - conf.bcx_1) < eps);
      accessor_has_bcy_0.write(p, conf.bcy_n > 0 && fabs(px_y[p] - conf.bcy_0) < eps);
      accessor_has_bcy_1.write(p, conf.bcy_n > 1 && fabs(px_y[p] - conf.bcy_1) < eps);
    }

    intptr_t pcolors_words = (conf.npieces + pcolors_bits - 1)/pcolors_bits;
    std::vector<intptr_t> pcolors(conf.np, nocolors);
    for (intptr_t m = 0; m < conf.npieces; m++) {
      intptr_t zstart = m_zstart[m], zend = m_zend[m];
      intptr_t zcolor = m; // zone color is the same as mesh piece
      for (intptr_t z = zstart; z < zend; z++) {
        for (std::vector<intptr_t>::iterator ip = mapzp[z].begin(), ep = mapzp[z].end(); ip != ep; ip++) {
          intptr_t p = *ip;

          if (pcolors[p] == nocolors || pcolors[p] == zcolor) {
            pcolors[p] = zcolor;
          } else {
            pcolors[p] = manycolors;
          }

          intptr_t word = p*pcolors_words + zcolor/pcolors_bits;
          intptr_t bit = zcolor%pcolors_bits;
          accessor_pcolors.write(word, accessor_pcolors.read(word) | static_cast<uint64_t>(1 << bit));
        }
      }
    }

    for (intptr_t p = 0; p < conf.np; p++) {
      intptr_t c = pcolors[p];
      assert(c != nocolors);
      accessor_pcolor.write(p, pcolors[p]);
    }

    for (intptr_t p = 0; p < conf.np; p++) {
      accessor_pcolor_shared.write(p, nocolors);
    }

    std::vector<bool> hascolor(conf.np, false);
    bool completely_exhausted = false;
    while (!completely_exhausted) {
      completely_exhausted = true;
      for (intptr_t m = 0; m < conf.npieces; m++) {
        intptr_t zstart = m_zstart[m], zend = m_zend[m];
        intptr_t zcolor = m; // zone color is the same as mesh piece
        bool has_new_point = false;
        for (intptr_t z = zstart; z < zend && !has_new_point; z++) {
          for (std::vector<intptr_t>::iterator ip = mapzp[z].begin(), ep = mapzp[z].end(); ip != ep; ip++) {
            intptr_t p = *ip;

            if (pcolors[p] == manycolors) {
              if (!hascolor[p]) {
                accessor_pcolor_shared.write(p, zcolor);
                hascolor[p] = true;
                has_new_point = true;
                completely_exhausted = false;
                break;
              }
            }
          }
        }
      }
    }
  }

  // Initialize sides.
  {
    RegionAccessor<AOS_SIDES, ptr_t> accessor_mapsz =
      rs_all[0].get_field_accessor(FIELD_MAPSZ).typeify<ptr_t>().convert<AOS_SIDES>();
    RegionAccessor<AOS_SIDES, ptr_t> accessor_mapsp1_pointer =
      rs_all[0].get_field_accessor(FIELD_MAPSP1_POINTER).typeify<ptr_t>().convert<AOS_SIDES>();
    RegionAccessor<AOS_SIDES, uint32_t> accessor_mapsp1_region =
      rs_all[0].get_field_accessor(FIELD_MAPSP1_REGION).typeify<uint32_t>().convert<AOS_SIDES>();
    RegionAccessor<AOS_SIDES, ptr_t> accessor_mapsp2_pointer =
      rs_all[0].get_field_accessor(FIELD_MAPSP2_POINTER).typeify<ptr_t>().convert<AOS_SIDES>();
    RegionAccessor<AOS_SIDES, uint32_t> accessor_mapsp2_region =
      rs_all[0].get_field_accessor(FIELD_MAPSP2_REGION).typeify<uint32_t>().convert<AOS_SIDES>();
    RegionAccessor<AOS_SIDES, ptr_t> accessor_mapss3 =
      rs_all[0].get_field_accessor(FIELD_MAPSS3).typeify<ptr_t>().convert<AOS_SIDES>();
    RegionAccessor<AOS_SIDES, ptr_t> accessor_mapss4 =
      rs_all[0].get_field_accessor(FIELD_MAPSS4).typeify<ptr_t>().convert<AOS_SIDES>();

    for (intptr_t m = 0; m < conf.npieces; m++) {
      intptr_t zstart = m_zstart[m], zend = m_zend[m];
      intptr_t sstart = m_sstart[m];
      for (intptr_t z = zstart; z < zend; z++) {
        intptr_t znump = mapzp[z].size();
        for (intptr_t is = 0; is < znump; is++) {
          intptr_t is3 = (is + znump - 1)%znump;
          intptr_t is4 = (is + 1)%znump;

          intptr_t s = is + sstart;
          intptr_t s3 = is3 + sstart;
          intptr_t s4 = is4 + sstart;
          intptr_t p1 = mapzp[z][is];
          intptr_t p2 = mapzp[z][is4];

          accessor_mapsz.write(s, z);
          accessor_mapsp1_pointer.write(s, p1);
          accessor_mapsp1_region.write(s, 0);
          accessor_mapsp2_pointer.write(s, p2);
          accessor_mapsp2_region.write(s, 0);
          accessor_mapss3.write(s, s3);
          accessor_mapss4.write(s, s4);
        }
        sstart += znump;
      }
    }
  }

  // Initialize mesh pieces.
  {
    RegionAccessor<AccessorType::AOS<0>, intptr_t> accessor_mcolor =
      rm_all[0].get_field_accessor(FIELD_MCOLOR).typeify<intptr_t>().convert<AccessorType::AOS<0> >();
    RegionAccessor<AccessorType::AOS<0>, intptr_t> accessor_zstart =
      rm_all[0].get_field_accessor(FIELD_ZSTART).typeify<intptr_t>().convert<AccessorType::AOS<0> >();
    RegionAccessor<AccessorType::AOS<0>, intptr_t> accessor_zend =
      rm_all[0].get_field_accessor(FIELD_ZEND).typeify<intptr_t>().convert<AccessorType::AOS<0> >();
    RegionAccessor<AccessorType::AOS<0>, intptr_t> accessor_sstart =
      rm_all[0].get_field_accessor(FIELD_SSTART).typeify<intptr_t>().convert<AccessorType::AOS<0> >();
    RegionAccessor<AccessorType::AOS<0>, intptr_t> accessor_send =
      rm_all[0].get_field_accessor(FIELD_SEND).typeify<intptr_t>().convert<AccessorType::AOS<0> >();

    for (intptr_t m = 0; m < conf.npieces; m++) {
      accessor_mcolor.write(m, m);
      accessor_zstart.write(m, m_zstart[m]);
      accessor_zend.write(m, m_zend[m]);
      accessor_sstart.write(m, m_sstart[m]);
      accessor_send.write(m, m_send[m]);
    }
  }

}

void write_output(HighLevelRuntime *runtime,
                  Context ctx,
                  config conf,
                  PhysicalRegion rz_all[1],
                  PhysicalRegion rp_all[1],
                  PhysicalRegion rs_all[1])
{
  // state *ctx = (state *)raw_ctx;
  // printf("#%4s\n", "zr");
  // for (int iz = 0; iz < ctx->conf.nz; iz++) {
  //   printf("%5d%18.8e\n", iz + 1, ctx->zr[iz]);
  // }
  // printf("#  ze\n");
  // for (int iz = 0; iz < ctx->conf.nz; iz++) {
  //   printf("%5d%18.8e\n", iz + 1, ctx->ze[iz]);
  // }
  // printf("#  zp\n");
  // for (int iz = 0; iz < ctx->conf.nz; iz++) {
  //   printf("%5d%18.8e\n", iz + 1, ctx->zp[iz]);
  // }
}

void foreign_validate_output(HighLevelRuntime *runtime,
                             Context ctx,
                             config conf,
                             PhysicalRegion rz_all[1],
                             PhysicalRegion rp_all[1],
                             PhysicalRegion rs_all[1])
{
  std::vector<double> sol_zr, sol_ze, sol_zp;

  {
    std::string xy_filename = get_solution_filename();
    std::ifstream xy_file(xy_filename.c_str(), std::ios::in);
    if (!xy_file) {
      assert(0 && "solution file does not exist");
    }

    {
      std::string line;
      getline(xy_file, line);
      assert(line == "#  zr");
    }

    for (int i = 0; i < conf.nz; i++) {
      int iz;
      double zr;
      xy_file >> iz;
      xy_file >> zr;
      sol_zr.push_back(zr);
    }

    {
      std::string ignore;
      getline(xy_file, ignore);
      std::string line;
      getline(xy_file, line);
      assert(line == "#  ze");
    }

    for (int i = 0; i < conf.nz; i++) {
      int iz;
      double ze;
      xy_file >> iz;
      xy_file >> ze;
      sol_ze.push_back(ze);
    }

    {
      std::string ignore;
      getline(xy_file, ignore);
      std::string line;
      getline(xy_file, line);
      assert(line == "#  zp");
    }

    for (int i = 0; i < conf.nz; i++) {
      int iz;
      double zp;
      xy_file >> iz;
      xy_file >> zp;
      sol_zp.push_back(zp);
    }
  }

  RegionAccessor<AOS_ZONES, double> accessor_zr =
    rz_all[0].get_field_accessor(FIELD_ZR).typeify<double>().convert<AOS_ZONES>();
  RegionAccessor<AOS_ZONES, double> accessor_ze =
    rz_all[0].get_field_accessor(FIELD_ZE).typeify<double>().convert<AOS_ZONES>();
  RegionAccessor<AOS_ZONES, double> accessor_zp =
    rz_all[0].get_field_accessor(FIELD_ZP).typeify<double>().convert<AOS_ZONES>();

  double absolute_eps = 1e-12;
  double relative_eps = 1e-8;
  for (intptr_t iz = 0; iz < conf.nz; iz++) {
    double ck = accessor_zr.read(iz);
    double sol = sol_zr[iz];
    if (fabs(ck - sol) < absolute_eps) {
      continue;
    }
    if (fabs(ck - sol) / sol < relative_eps) {
      continue;
    }
    assert(0 && "zr value out of bounds");
  }

  for (intptr_t iz = 0; iz < conf.nz; iz++) {
    double ck = accessor_ze.read(iz);
    double sol = sol_ze[iz];
    if (fabs(ck - sol) < absolute_eps) {
      continue;
    }
    if (fabs(ck - sol) / sol < relative_eps) {
      continue;
    }
    assert(0 && "ze value out of bounds");
  }

  for (intptr_t iz = 0; iz < conf.nz; iz++) {
    double ck = accessor_zp.read(iz);
    double sol = sol_zp[iz];
    if (fabs(ck - sol) < absolute_eps) {
      continue;
    }
    if (fabs(ck - sol) / sol < relative_eps) {
      continue;
    }
    assert(0 && "zp value out of bounds");
  }
}

double get_abs_time()
{
  return LegionRuntime::LowLevel::Clock::abs_time();
}

void print_global_elapsed_time(double start_time, double end_time)
{
  double delta_time = end_time - start_time;
  printf("\n**************************************\n");
  printf("total problem run time=%15.6e\n", delta_time);
  printf("**************************************\n\n");
}

void print_simulation_start()
{
  printf("Starting simulation\n");
}

void print_simulation_loop(intptr_t cycle, double time, double dt,
                           double start_time, double last_time,
                           double current_time, intptr_t interval)
{
  printf("cycle %4ld    sim time %.3e    dt %.3e    time %.3e (per iteration) %.3e (total)\n",
         cycle, time, dt, (current_time - last_time)/interval, current_time - start_time);
}

///
/// Coloring
///

Coloring foreign_all_zones_coloring(HighLevelRuntime *runtime,
                                    Context ctx,
                                    config conf,
                                    PhysicalRegion rm_all[1])
{
  Coloring result;

  RegionAccessor<AccessorType::AOS<0>, intptr_t> accessor_zstart =
    rm_all[0].get_field_accessor(FIELD_ZSTART).typeify<intptr_t>().convert<AccessorType::AOS<0> >();
  RegionAccessor<AccessorType::AOS<0>, intptr_t> accessor_zend =
    rm_all[0].get_field_accessor(FIELD_ZEND).typeify<intptr_t>().convert<AccessorType::AOS<0> >();

  for (intptr_t m = 0; m < conf.npieces; m++) {
    intptr_t zstart = accessor_zstart.read(m);
    intptr_t zend = accessor_zend.read(m);
    result[m].ranges.insert(std::pair<ptr_t, ptr_t>(zstart, zend - 1));
  }

  return result;
}

Coloring foreign_all_points_coloring(HighLevelRuntime *runtime,
                                     Context ctx,
                                     config conf,
                                     PhysicalRegion pcolor_a[1])
{
  Coloring result;

  // Force all colors to exist, even if empty.
  result[0];
  result[1];

  RegionAccessor<AccessorType::AOS<0>, intptr_t> accessor_pcolor =
    pcolor_a[0].get_accessor().typeify<intptr_t>().convert<AccessorType::AOS<0> >();

  for (intptr_t p = 0; p < conf.np; p++) {
    intptr_t c = accessor_pcolor.read(p);
    if (c == manycolors) {
      result[1].points.insert(p);
    } else {
      result[0].points.insert(p);
    }
  }

  return result;
}

Coloring foreign_private_points_coloring(HighLevelRuntime *runtime,
                                         Context ctx,
                                         config conf,
                                         PhysicalRegion pcolor_a[1])
{
  Coloring result;

  // Force all colors to exist even if empty.
  for (intptr_t c = 0; c < conf.npieces; c++) {
    result[c];
  }

  RegionAccessor<AccessorType::AOS<0>, intptr_t> accessor_pcolor =
    pcolor_a[0].get_accessor().typeify<intptr_t>().convert<AccessorType::AOS<0> >();

  for (intptr_t p = 0; p < conf.np; p++) {
    intptr_t c = accessor_pcolor.read(p);
    assert(c != nocolors);
    if (c != manycolors) {
      result[c].points.insert(p);
    }
  }

  return result;
}

Coloring foreign_ghost_points_coloring(HighLevelRuntime *runtime,
                                       Context ctx,
                                       config conf,
                                       PhysicalRegion pcolor_a[1],
                                       PhysicalRegion pcolors_a[1])
{
  Coloring result;

  // Force all colors to exist even if empty.
  for (intptr_t c = 0; c < conf.npieces; c++) {
    result[c];
  }

  RegionAccessor<AccessorType::AOS<0>, intptr_t> accessor_pcolor =
    pcolor_a[0].get_accessor().typeify<intptr_t>().convert<AccessorType::AOS<0> >();

  RegionAccessor<AccessorType::AOS<0>, uint64_t> accessor_pcolors =
    pcolors_a[0].get_accessor().typeify<uint64_t>().convert<AccessorType::AOS<0> >();

  intptr_t pcolors_words = (conf.npieces + pcolors_bits - 1)/pcolors_bits;
  for (intptr_t p = 0; p < conf.np; p++) {
    intptr_t pcolors = accessor_pcolor.read(p);
    if (pcolors == manycolors) {
      for (intptr_t w = 0; w < pcolors_words; w++) {
        uint64_t word = accessor_pcolors.read(p*pcolors_words + w);
        for (intptr_t bit = 0; bit < pcolors_bits; bit++) {
          intptr_t c = w*pcolors_bits + bit;
          if (c >= conf.npieces) {
            break;
          }
          if (word & static_cast<uint64_t>(1 << bit)) {
            result[c].points.insert(p);
          }
        }
      }
    }
  }

  return result;
}

Coloring foreign_shared_points_coloring(HighLevelRuntime *runtime,
                                        Context ctx,
                                        config conf,
                                        PhysicalRegion pcolor_shared_a[1])
{
  Coloring result;

  // Force all colors to exist even if empty.
  for (intptr_t c = 0; c < conf.npieces; c++) {
    result[c];
  }

  RegionAccessor<AccessorType::AOS<0>, intptr_t> accessor_pcolor_shared =
    pcolor_shared_a[0].get_accessor().typeify<intptr_t>().convert<AccessorType::AOS<0> >();

  for (intptr_t p = 0; p < conf.np; p++) {
    intptr_t c = accessor_pcolor_shared.read(p);
    if (c != nocolors) {
      // printf("shared point %ld color %ld\n", p, c);
      result[c].points.insert(p);
    }
  }

  return result;
}

Coloring foreign_all_sides_coloring(HighLevelRuntime *runtime,
                                 Context ctx,
                                 config conf,
                                 PhysicalRegion rm_all[1])
{
  Coloring result;

  RegionAccessor<AccessorType::AOS<0>, intptr_t> accessor_sstart =
    rm_all[0].get_field_accessor(FIELD_SSTART).typeify<intptr_t>().convert<AccessorType::AOS<0> >();
  RegionAccessor<AccessorType::AOS<0>, intptr_t> accessor_send =
    rm_all[0].get_field_accessor(FIELD_SEND).typeify<intptr_t>().convert<AccessorType::AOS<0> >();

  for (intptr_t m = 0; m < conf.npieces; m++) {
    intptr_t sstart = accessor_sstart.read(m);
    intptr_t send = accessor_send.read(m);
    result[m].ranges.insert(std::pair<ptr_t, ptr_t>(sstart, send - 1));
  }

  return result;
}

///
/// Kernels
///

void foreign_init_step_zones(HighLevelRuntime *runtime,
                        Context ctx,
                        intptr_t zstart,
                        intptr_t zend,
                        PhysicalRegion rz[2])
{
#define USE_RAW 0
#if !USE_RAW
  RegionAccessor<AOS_ZONES, double> accessor_zvol =
    rz[0].get_field_accessor(FIELD_ZVOL).typeify<double>().convert<AOS_ZONES>();
  RegionAccessor<AOS_ZONES, double> accessor_zxp_x =
    rz[1].get_field_accessor(FIELD_ZXP_X).typeify<double>().convert<AOS_ZONES>();
  RegionAccessor<AOS_ZONES, double> accessor_zxp_y =
    rz[1].get_field_accessor(FIELD_ZXP_Y).typeify<double>().convert<AOS_ZONES>();
  RegionAccessor<AOS_ZONES, double> accessor_zx_x =
    rz[1].get_field_accessor(FIELD_ZX_X).typeify<double>().convert<AOS_ZONES>();
  RegionAccessor<AOS_ZONES, double> accessor_zx_y =
    rz[1].get_field_accessor(FIELD_ZX_Y).typeify<double>().convert<AOS_ZONES>();
  RegionAccessor<AOS_ZONES, double> accessor_zareap =
    rz[1].get_field_accessor(FIELD_ZAREAP).typeify<double>().convert<AOS_ZONES>();
  RegionAccessor<AOS_ZONES, double> accessor_zarea =
    rz[1].get_field_accessor(FIELD_ZAREA).typeify<double>().convert<AOS_ZONES>();
  RegionAccessor<AOS_ZONES, double> accessor_zvol0 =
    rz[1].get_field_accessor(FIELD_ZVOL0).typeify<double>().convert<AOS_ZONES>();
  RegionAccessor<AOS_ZONES, double> accessor_zvolp =
    rz[1].get_field_accessor(FIELD_ZVOLP).typeify<double>().convert<AOS_ZONES>();
  RegionAccessor<AOS_ZONES, double> accessor_zdl =
    rz[1].get_field_accessor(FIELD_ZDL).typeify<double>().convert<AOS_ZONES>();
  RegionAccessor<AOS_ZONES, double> accessor_zw =
    rz[1].get_field_accessor(FIELD_ZW).typeify<double>().convert<AOS_ZONES>();
  RegionAccessor<AOS_ZONES, double> accessor_zuc_x =
    rz[1].get_field_accessor(FIELD_ZUC_X).typeify<double>().convert<AOS_ZONES>();
  RegionAccessor<AOS_ZONES, double> accessor_zuc_y =
    rz[1].get_field_accessor(FIELD_ZUC_Y).typeify<double>().convert<AOS_ZONES>();
#else
  int count0, count;
  ByteOffset offset0, offset;
  RegionAccessor<AccessorType::Generic, double> accessor_zvol =
    rz[0].get_field_accessor(FIELD_ZVOL).typeify<double>();
  double *raw_zvol = accessor_zvol.raw_dense_ptr<0>(zstart, count0, offset0);

  RegionAccessor<AccessorType::Generic, double> accessor_zxp_x =
    rz[1].get_field_accessor(FIELD_ZXP_X).typeify<double>();
  double *raw_zxp_x = accessor_zxp_x.raw_dense_ptr<0>(zstart, count, offset);
  assert(count == count0 && offset == offset0);

  RegionAccessor<AccessorType::Generic, double> accessor_zxp_y =
    rz[1].get_field_accessor(FIELD_ZXP_Y).typeify<double>();
  double *raw_zxp_y = accessor_zxp_y.raw_dense_ptr<0>(zstart, count, offset);
  assert(count == count0 && offset == offset0);

  RegionAccessor<AccessorType::Generic, double> accessor_zx_x =
    rz[1].get_field_accessor(FIELD_ZX_X).typeify<double>();
  double *raw_zx_x = accessor_zx_x.raw_dense_ptr<0>(zstart, count, offset);
  assert(count == count0 && offset == offset0);

  RegionAccessor<AccessorType::Generic, double> accessor_zx_y =
    rz[1].get_field_accessor(FIELD_ZX_Y).typeify<double>();
  double *raw_zx_y = accessor_zx_y.raw_dense_ptr<0>(zstart, count, offset);
  assert(count == count0 && offset == offset0);

  RegionAccessor<AccessorType::Generic, double> accessor_zareap =
    rz[1].get_field_accessor(FIELD_ZAREAP).typeify<double>();
  double *raw_zareap = accessor_zareap.raw_dense_ptr<0>(zstart, count, offset);
  assert(count == count0 && offset == offset0);

  RegionAccessor<AccessorType::Generic, double> accessor_zarea =
    rz[1].get_field_accessor(FIELD_ZAREA).typeify<double>();
  double *raw_zarea = accessor_zarea.raw_dense_ptr<0>(zstart, count, offset);
  assert(count == count0 && offset == offset0);

  RegionAccessor<AccessorType::Generic, double> accessor_zvol0 =
    rz[1].get_field_accessor(FIELD_ZVOL0).typeify<double>();
  double *raw_zvol0 = accessor_zvol0.raw_dense_ptr<0>(zstart, count, offset);
  assert(count == count0 && offset == offset0);

  RegionAccessor<AccessorType::Generic, double> accessor_zvolp =
    rz[1].get_field_accessor(FIELD_ZVOLP).typeify<double>();
  double *raw_zvolp = accessor_zvolp.raw_dense_ptr<0>(zstart, count, offset);
  assert(count == count0 && offset == offset0);

  RegionAccessor<AccessorType::Generic, double> accessor_zdl =
    rz[1].get_field_accessor(FIELD_ZDL).typeify<double>();
  double *raw_zdl = accessor_zdl.raw_dense_ptr<0>(zstart, count, offset);
  assert(count == count0 && offset == offset0);

  RegionAccessor<AccessorType::Generic, double> accessor_zw =
    rz[1].get_field_accessor(FIELD_ZW).typeify<double>();
  double *raw_zw = accessor_zw.raw_dense_ptr<0>(zstart, count, offset);
  assert(count == count0 && offset == offset0);

  RegionAccessor<AccessorType::Generic, double> accessor_zuc_x =
    rz[1].get_field_accessor(FIELD_ZUC_X).typeify<double>();
  double *raw_zuc_x = accessor_zuc_x.raw_dense_ptr<0>(zstart, count, offset);
  assert(count == count0 && offset == offset0);

  RegionAccessor<AccessorType::Generic, double> accessor_zuc_y =
    rz[1].get_field_accessor(FIELD_ZUC_Y).typeify<double>();
  double *raw_zuc_y = accessor_zuc_y.raw_dense_ptr<0>(zstart, count, offset);
  assert(count == count0 && offset == offset0);
#endif

  // double start_time = get_abs_time();
  for (intptr_t z = zstart; z < zend; z++) {
#if !USE_RAW
    accessor_zxp_x.write(z, 0.0);
    accessor_zxp_y.write(z, 0.0);
    accessor_zx_x.write(z, 0.0);
    accessor_zx_y.write(z, 0.0);
    accessor_zareap.write(z, 0.0);
    accessor_zarea.write(z, 0.0);
    accessor_zvol0.write(z, accessor_zvol.read(z));
    accessor_zvolp.write(z, 0.0);
    accessor_zvol.write(z, 0.0);
    accessor_zdl.write(z, 1e99);
    accessor_zw.write(z, 0.0);
    accessor_zuc_x.write(z, 0.0);
    accessor_zuc_y.write(z, 0.0);
#else
    *raw_zxp_x = 0.0;
    *raw_zxp_y = 0.0;
    *raw_zx_x = 0.0;
    *raw_zx_y = 0.0;
    *raw_zareap = 0.0;
    *raw_zarea = 0.0;
    *raw_zvol0 = *raw_zvol;
    *raw_zvolp = 0.0;
    *raw_zvol = 0.0;
    *raw_zdl = 1e99;
    *raw_zw = 0.0;
    *raw_zuc_x = 0.0;
    *raw_zuc_y = 0.0;

    raw_zxp_x += offset;
    raw_zxp_y += offset;
    raw_zx_x += offset;
    raw_zx_y += offset;
    raw_zareap += offset;
    raw_zarea += offset;
    raw_zvol0 += offset;
    raw_zvolp += offset;
    raw_zvol += offset;
    raw_zdl += offset;
    raw_zw += offset;
    raw_zuc_x += offset;
    raw_zuc_y += offset;
#endif
  }
  // double end_time = get_abs_time();

  // printf("time in init_step_zones %e\n", end_time - start_time);
}

void foreign_calc_centers(HighLevelRuntime *runtime,
                          Context ctx,
                          intptr_t sstart,
                          intptr_t send,
                          PhysicalRegion rz[2],
                          PhysicalRegion rpp[1],
                          PhysicalRegion rpg[1],
                          PhysicalRegion rs[2])
{
#undef USE_RAW
#define USE_RAW 1
#if !USE_RAW
  RegionAccessor<AOS_ZONES, intptr_t> accessor_znump =
    rz[0].get_field_accessor(FIELD_ZNUMP).typeify<intptr_t>().convert<AOS_ZONES>();
  RegionAccessor<AOS_ZONES, double> accessor_zxp_x =
    rz[1].get_field_accessor(FIELD_ZXP_X).typeify<double>().convert<AOS_ZONES>();
  RegionAccessor<AOS_ZONES, double> accessor_zxp_y =
    rz[1].get_field_accessor(FIELD_ZXP_Y).typeify<double>().convert<AOS_ZONES>();

  RegionAccessor<AOS_POINTS, double> accessor_rpp_pxp_x =
    rpp[0].get_field_accessor(FIELD_PXP_X).typeify<double>().convert<AOS_POINTS>();
  RegionAccessor<AOS_POINTS, double> accessor_rpp_pxp_y =
    rpp[0].get_field_accessor(FIELD_PXP_Y).typeify<double>().convert<AOS_POINTS>();

  RegionAccessor<AOS_POINTS, double> accessor_rpg_pxp_x =
    rpg[0].get_field_accessor(FIELD_PXP_X).typeify<double>().convert<AOS_POINTS>();
  RegionAccessor<AOS_POINTS, double> accessor_rpg_pxp_y =
    rpg[0].get_field_accessor(FIELD_PXP_Y).typeify<double>().convert<AOS_POINTS>();

  RegionAccessor<AOS_SIDES, ptr_t> accessor_mapsz =
    rs[0].get_field_accessor(FIELD_MAPSZ).typeify<ptr_t>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, ptr_t> accessor_mapsp1_pointer =
    rs[0].get_field_accessor(FIELD_MAPSP1_POINTER).typeify<ptr_t>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, uint32_t> accessor_mapsp1_region =
    rs[0].get_field_accessor(FIELD_MAPSP1_REGION).typeify<uint32_t>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, ptr_t> accessor_mapsp2_pointer =
    rs[0].get_field_accessor(FIELD_MAPSP2_POINTER).typeify<ptr_t>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, uint32_t> accessor_mapsp2_region =
    rs[0].get_field_accessor(FIELD_MAPSP2_REGION).typeify<uint32_t>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, double> accessor_exp_x =
    rs[1].get_field_accessor(FIELD_EXP_X).typeify<double>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, double> accessor_exp_y =
    rs[1].get_field_accessor(FIELD_EXP_Y).typeify<double>().convert<AOS_SIDES>();
#else
  int zcount0, zcount;
  ByteOffset zstride0, zstride;
  RegionAccessor<AccessorType::Generic, intptr_t> accessor_znump =
    rz[0].get_field_accessor(FIELD_ZNUMP).typeify<intptr_t>();
  intptr_t *raw_znump = accessor_znump.raw_dense_ptr<0>(0, zcount0, zstride0);

  RegionAccessor<AccessorType::Generic, double> accessor_zxp_x =
    rz[1].get_field_accessor(FIELD_ZXP_X).typeify<double>();
  double *raw_zxp_x = accessor_zxp_x.raw_dense_ptr<0>(0, zcount, zstride);
  assert(zcount == zcount0 && zstride == zstride0);

  RegionAccessor<AccessorType::Generic, double> accessor_zxp_y =
    rz[1].get_field_accessor(FIELD_ZXP_Y).typeify<double>();
  double *raw_zxp_y = accessor_zxp_y.raw_dense_ptr<0>(0, zcount, zstride);
  assert(zcount == zcount0 && zstride == zstride0);

  int rppcount0, rppcount;
  ByteOffset rppstride0, rppstride;
  RegionAccessor<AccessorType::Generic, double> accessor_rpp_pxp_x =
    rpp[0].get_field_accessor(FIELD_PXP_X).typeify<double>();
  double *raw_rpp_pxp_x = accessor_rpp_pxp_x.raw_dense_ptr<0>(0, rppcount0, rppstride0);

  RegionAccessor<AccessorType::Generic, double> accessor_rpp_pxp_y =
    rpp[0].get_field_accessor(FIELD_PXP_Y).typeify<double>();
  double *raw_rpp_pxp_y = accessor_rpp_pxp_y.raw_dense_ptr<0>(0, rppcount, rppstride);
  assert(rppcount == rppcount0 && rppstride == rppstride0);

  int rpgcount0, rpgcount;
  ByteOffset rpgstride0, rpgstride;
  RegionAccessor<AccessorType::Generic, double> accessor_rpg_pxp_x =
    rpg[0].get_field_accessor(FIELD_PXP_X).typeify<double>();
  double *raw_rpg_pxp_x = accessor_rpg_pxp_x.raw_dense_ptr<0>(0, rpgcount0, rpgstride0);

  RegionAccessor<AccessorType::Generic, double> accessor_rpg_pxp_y =
    rpg[0].get_field_accessor(FIELD_PXP_Y).typeify<double>();
  double *raw_rpg_pxp_y = accessor_rpg_pxp_y.raw_dense_ptr<0>(0, rpgcount, rpgstride);
  assert(rpgcount == rpgcount0 && rpgstride == rpgstride0);

  int scount0, scount;
  ByteOffset sstride0, sstride;
  RegionAccessor<AccessorType::Generic, ptr_t> accessor_mapsz =
    rs[0].get_field_accessor(FIELD_MAPSZ).typeify<ptr_t>();
  ptr_t *raw_mapsz = accessor_mapsz.raw_dense_ptr<0>(0, scount0, sstride0);

  RegionAccessor<AccessorType::Generic, ptr_t> accessor_mapsp1_pointer =
    rs[0].get_field_accessor(FIELD_MAPSP1_POINTER).typeify<ptr_t>();
  ptr_t *raw_mapsp1_pointer = accessor_mapsp1_pointer.raw_dense_ptr<0>(0, scount, sstride);
  assert(scount == scount0 && sstride == sstride0);

  RegionAccessor<AccessorType::Generic, uint32_t> accessor_mapsp1_region =
    rs[0].get_field_accessor(FIELD_MAPSP1_REGION).typeify<uint32_t>();
  uint32_t *raw_mapsp1_region = accessor_mapsp1_region.raw_dense_ptr<0>(0, scount, sstride);
  assert(scount == scount0 && sstride == sstride0);

  RegionAccessor<AccessorType::Generic, ptr_t> accessor_mapsp2_pointer =
    rs[0].get_field_accessor(FIELD_MAPSP2_POINTER).typeify<ptr_t>();
  ptr_t *raw_mapsp2_pointer = accessor_mapsp2_pointer.raw_dense_ptr<0>(0, scount, sstride);
  assert(scount == scount0 && sstride == sstride0);

  RegionAccessor<AccessorType::Generic, uint32_t> accessor_mapsp2_region =
    rs[0].get_field_accessor(FIELD_MAPSP2_REGION).typeify<uint32_t>();
  uint32_t *raw_mapsp2_region = accessor_mapsp2_region.raw_dense_ptr<0>(0, scount, sstride);
  assert(scount == scount0 && sstride == sstride0);

  RegionAccessor<AccessorType::Generic, double> accessor_exp_x =
    rs[1].get_field_accessor(FIELD_EXP_X).typeify<double>();
  double *raw_exp_x = accessor_exp_x.raw_dense_ptr<0>(0, scount, sstride);
  assert(scount == scount0 && sstride == sstride0);

  RegionAccessor<AccessorType::Generic, double> accessor_exp_y =
    rs[1].get_field_accessor(FIELD_EXP_Y).typeify<double>();
  double *raw_exp_y = accessor_exp_y.raw_dense_ptr<0>(0, scount, sstride);
  assert(scount == scount0 && sstride == sstride0);
#endif

  double start_time = get_abs_time();
#if USE_RAW
  ByteOffset soffset = sstart*sstride;
#endif
  for (intptr_t s = sstart; s < send; s++) {
#if !USE_RAW
    ptr_t z = accessor_mapsz.read(s);
    ptr_t p1_pointer = accessor_mapsp1_pointer.read(s);
    uint32_t p1_region = accessor_mapsp1_region.read(s);
    ptr_t p2_pointer = accessor_mapsp2_pointer.read(s);
    uint32_t p2_region = accessor_mapsp2_region.read(s);

    vec2 p1_pxp;
    switch (p1_region) {
    case 1:
      p1_pxp.x = accessor_rpp_pxp_x.read(p1_pointer);
      p1_pxp.y = accessor_rpp_pxp_y.read(p1_pointer);
      break;
    case 2:
      p1_pxp.x = accessor_rpg_pxp_x.read(p1_pointer);
      p1_pxp.y = accessor_rpg_pxp_y.read(p1_pointer);
      break;
    }

    vec2 p2_pxp;
    switch (p2_region) {
    case 1:
      p2_pxp.x = accessor_rpp_pxp_x.read(p2_pointer);
      p2_pxp.y = accessor_rpp_pxp_y.read(p2_pointer);
      break;
    case 2:
      p2_pxp.x = accessor_rpg_pxp_x.read(p2_pointer);
      p2_pxp.y = accessor_rpg_pxp_y.read(p2_pointer);
      break;
    }

    vec2 exp = scale(add(p1_pxp, p2_pxp), 0.5);
    accessor_exp_x.write(s, exp.x);
    accessor_exp_y.write(s, exp.y);

    double znump = static_cast<double>(accessor_znump.read(z));
    accessor_zxp_x.write(z, accessor_zxp_x.read(z) + p1_pxp.x/znump);
    accessor_zxp_y.write(z, accessor_zxp_y.read(z) + p1_pxp.y/znump);
#else
    ptr_t z = *(raw_mapsz + soffset);
    ptr_t p1_pointer = *(raw_mapsp1_pointer + soffset);
    uint32_t p1_region = *(raw_mapsp1_region + soffset);
    ptr_t p2_pointer = *(raw_mapsp2_pointer + soffset);
    uint32_t p2_region = *(raw_mapsp2_region + soffset);

    vec2 p1_pxp;
    switch (p1_region) {
    case 1:
      p1_pxp.x = *(raw_rpp_pxp_x + p1_pointer*rppstride);
      p1_pxp.y = *(raw_rpp_pxp_y + p1_pointer*rppstride);
      break;
    case 2:
      p1_pxp.x = *(raw_rpg_pxp_x + p1_pointer*rpgstride);
      p1_pxp.y = *(raw_rpg_pxp_y + p1_pointer*rpgstride);
      break;
    }

    vec2 p2_pxp;
    switch (p2_region) {
    case 1:
      p2_pxp.x = *(raw_rpp_pxp_x + p2_pointer*rppstride);
      p2_pxp.y = *(raw_rpp_pxp_y + p2_pointer*rppstride);
      break;
    case 2:
      p2_pxp.x = *(raw_rpg_pxp_x + p2_pointer*rpgstride);
      p2_pxp.y = *(raw_rpg_pxp_y + p2_pointer*rpgstride);
      break;
    }

    vec2 exp = scale(add(p1_pxp, p2_pxp), 0.5);
    *(raw_exp_x + soffset) = exp.x;
    *(raw_exp_y + soffset) = exp.y;

    double znump = static_cast<double>(*(raw_znump + z*zstride));
    *(raw_zxp_x + z*zstride) += p1_pxp.x/znump;
    *(raw_zxp_y + z*zstride) += p1_pxp.y/znump;

    soffset += sstride;
#endif
  }
  double end_time = get_abs_time();

  printf("time in calc_centers %e\n", end_time - start_time);
}

void foreign_calc_volumes(HighLevelRuntime *runtime,
                          Context ctx,
                          intptr_t sstart,
                          intptr_t send,
                          PhysicalRegion rz[2],
                          PhysicalRegion rpp[1],
                          PhysicalRegion rpg[1],
                          PhysicalRegion rs[2])
{
  RegionAccessor<AOS_ZONES, double> accessor_zxp_x =
    rz[0].get_field_accessor(FIELD_ZXP_X).typeify<double>().convert<AOS_ZONES>();
  RegionAccessor<AOS_ZONES, double> accessor_zxp_y =
    rz[0].get_field_accessor(FIELD_ZXP_Y).typeify<double>().convert<AOS_ZONES>();
  RegionAccessor<AOS_ZONES, double> accessor_zareap =
    rz[1].get_field_accessor(FIELD_ZAREAP).typeify<double>().convert<AOS_ZONES>();
  RegionAccessor<AOS_ZONES, double> accessor_zvolp =
    rz[1].get_field_accessor(FIELD_ZVOLP).typeify<double>().convert<AOS_ZONES>();

  RegionAccessor<AOS_POINTS, double> accessor_rpp_pxp_x =
    rpp[0].get_field_accessor(FIELD_PXP_X).typeify<double>().convert<AOS_POINTS>();
  RegionAccessor<AOS_POINTS, double> accessor_rpp_pxp_y =
    rpp[0].get_field_accessor(FIELD_PXP_Y).typeify<double>().convert<AOS_POINTS>();

  RegionAccessor<AOS_POINTS, double> accessor_rpg_pxp_x =
    rpg[0].get_field_accessor(FIELD_PXP_X).typeify<double>().convert<AOS_POINTS>();
  RegionAccessor<AOS_POINTS, double> accessor_rpg_pxp_y =
    rpg[0].get_field_accessor(FIELD_PXP_Y).typeify<double>().convert<AOS_POINTS>();

  RegionAccessor<AOS_SIDES, ptr_t> accessor_mapsz =
    rs[0].get_field_accessor(FIELD_MAPSZ).typeify<ptr_t>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, ptr_t> accessor_mapsp1_pointer =
    rs[0].get_field_accessor(FIELD_MAPSP1_POINTER).typeify<ptr_t>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, uint32_t> accessor_mapsp1_region =
    rs[0].get_field_accessor(FIELD_MAPSP1_REGION).typeify<uint32_t>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, ptr_t> accessor_mapsp2_pointer =
    rs[0].get_field_accessor(FIELD_MAPSP2_POINTER).typeify<ptr_t>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, uint32_t> accessor_mapsp2_region =
    rs[0].get_field_accessor(FIELD_MAPSP2_REGION).typeify<uint32_t>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, double> accessor_sareap =
    rs[1].get_field_accessor(FIELD_SAREAP).typeify<double>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, double> accessor_svolp =
    rs[1].get_field_accessor(FIELD_SVOLP).typeify<double>().convert<AOS_SIDES>();

  for (intptr_t s = sstart; s < send; s++) {
    ptr_t z = accessor_mapsz.read(s);
    ptr_t p1_pointer = accessor_mapsp1_pointer.read(s);
    uint32_t p1_region = accessor_mapsp1_region.read(s);
    ptr_t p2_pointer = accessor_mapsp2_pointer.read(s);
    uint32_t p2_region = accessor_mapsp2_region.read(s);

    vec2 zxp;
    zxp.x = accessor_zxp_x.read(z);
    zxp.y = accessor_zxp_y.read(z);

    vec2 p1_pxp;
    switch (p1_region) {
    case 1:
      p1_pxp.x = accessor_rpp_pxp_x.read(p1_pointer);
      p1_pxp.y = accessor_rpp_pxp_y.read(p1_pointer);
      break;
    case 2:
      p1_pxp.x = accessor_rpg_pxp_x.read(p1_pointer);
      p1_pxp.y = accessor_rpg_pxp_y.read(p1_pointer);
      break;
    }

    vec2 p2_pxp;
    switch (p2_region) {
    case 1:
      p2_pxp.x = accessor_rpp_pxp_x.read(p2_pointer);
      p2_pxp.y = accessor_rpp_pxp_y.read(p2_pointer);
      break;
    case 2:
      p2_pxp.x = accessor_rpg_pxp_x.read(p2_pointer);
      p2_pxp.y = accessor_rpg_pxp_y.read(p2_pointer);
      break;
    }

    double sa = 0.5 * cross(sub(p2_pxp, p1_pxp), sub(zxp, p1_pxp));
    double sv = (1.0 / 3.0) * sa * (p1_pxp.x + p2_pxp.x + zxp.x);

    accessor_sareap.write(s, sa);
    accessor_svolp.write(s, sv);
    accessor_zareap.write(z, accessor_zareap.read(z) + sa);
    accessor_zvolp.write(z, accessor_zvolp.read(z) + sv);
    assert(sv > 0.0);
  }
}

void foreign_calc_surface_vecs(HighLevelRuntime *runtime,
                               Context ctx,
                               intptr_t sstart,
                               intptr_t send,
                               PhysicalRegion rz[1],
                               PhysicalRegion rs[2])
{
  RegionAccessor<AOS_ZONES, double> accessor_zxp_x =
    rz[0].get_field_accessor(FIELD_ZXP_X).typeify<double>().convert<AOS_ZONES>();
  RegionAccessor<AOS_ZONES, double> accessor_zxp_y =
    rz[0].get_field_accessor(FIELD_ZXP_Y).typeify<double>().convert<AOS_ZONES>();

  RegionAccessor<AOS_SIDES, ptr_t> accessor_mapsz =
    rs[0].get_field_accessor(FIELD_MAPSZ).typeify<ptr_t>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, double> accessor_exp_x =
    rs[0].get_field_accessor(FIELD_EXP_X).typeify<double>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, double> accessor_exp_y =
    rs[0].get_field_accessor(FIELD_EXP_Y).typeify<double>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, double> accessor_ssurfp_x =
    rs[1].get_field_accessor(FIELD_SSURFP_X).typeify<double>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, double> accessor_ssurfp_y =
    rs[1].get_field_accessor(FIELD_SSURFP_Y).typeify<double>().convert<AOS_SIDES>();

  for (intptr_t s = sstart; s < send; s++) {
    ptr_t z = accessor_mapsz.read(s);

    vec2 exp;
    exp.x = accessor_exp_x.read(s);
    exp.y = accessor_exp_y.read(s);

    vec2 zxp;
    zxp.x = accessor_zxp_x.read(z);
    zxp.y = accessor_zxp_y.read(z);

    vec2 ssurfp = rotateCCW(sub(exp, zxp));
    accessor_ssurfp_x.write(s, ssurfp.x);
    accessor_ssurfp_y.write(s, ssurfp.y);
  }
}

void foreign_calc_edge_len(HighLevelRuntime *runtime,
                           Context ctx,
                           intptr_t sstart,
                           intptr_t send,
                           PhysicalRegion rpp[1],
                           PhysicalRegion rpg[1],
                           PhysicalRegion rs[2])
{
  RegionAccessor<AOS_POINTS, double> accessor_rpp_pxp_x =
    rpp[0].get_field_accessor(FIELD_PXP_X).typeify<double>().convert<AOS_POINTS>();
  RegionAccessor<AOS_POINTS, double> accessor_rpp_pxp_y =
    rpp[0].get_field_accessor(FIELD_PXP_Y).typeify<double>().convert<AOS_POINTS>();

  RegionAccessor<AOS_POINTS, double> accessor_rpg_pxp_x =
    rpg[0].get_field_accessor(FIELD_PXP_X).typeify<double>().convert<AOS_POINTS>();
  RegionAccessor<AOS_POINTS, double> accessor_rpg_pxp_y =
    rpg[0].get_field_accessor(FIELD_PXP_Y).typeify<double>().convert<AOS_POINTS>();

  RegionAccessor<AOS_SIDES, ptr_t> accessor_mapsp1_pointer =
    rs[0].get_field_accessor(FIELD_MAPSP1_POINTER).typeify<ptr_t>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, uint32_t> accessor_mapsp1_region =
    rs[0].get_field_accessor(FIELD_MAPSP1_REGION).typeify<uint32_t>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, ptr_t> accessor_mapsp2_pointer =
    rs[0].get_field_accessor(FIELD_MAPSP2_POINTER).typeify<ptr_t>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, uint32_t> accessor_mapsp2_region =
    rs[0].get_field_accessor(FIELD_MAPSP2_REGION).typeify<uint32_t>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, double> accessor_elen =
    rs[1].get_field_accessor(FIELD_ELEN).typeify<double>().convert<AOS_SIDES>();

  for (intptr_t s = sstart; s < send; s++) {
    ptr_t p1_pointer = accessor_mapsp1_pointer.read(s);
    uint32_t p1_region = accessor_mapsp1_region.read(s);
    ptr_t p2_pointer = accessor_mapsp2_pointer.read(s);
    uint32_t p2_region = accessor_mapsp2_region.read(s);

    vec2 p1_pxp;
    switch (p1_region) {
    case 1:
      p1_pxp.x = accessor_rpp_pxp_x.read(p1_pointer);
      p1_pxp.y = accessor_rpp_pxp_y.read(p1_pointer);
      break;
    case 2:
      p1_pxp.x = accessor_rpg_pxp_x.read(p1_pointer);
      p1_pxp.y = accessor_rpg_pxp_y.read(p1_pointer);
      break;
    }

    vec2 p2_pxp;
    switch (p2_region) {
    case 1:
      p2_pxp.x = accessor_rpp_pxp_x.read(p2_pointer);
      p2_pxp.y = accessor_rpp_pxp_y.read(p2_pointer);
      break;
    case 2:
      p2_pxp.x = accessor_rpg_pxp_x.read(p2_pointer);
      p2_pxp.y = accessor_rpg_pxp_y.read(p2_pointer);
      break;
    }

    double elen = length(sub(p2_pxp, p1_pxp));

    accessor_elen.write(s, elen);
  }
}

void foreign_calc_char_len(HighLevelRuntime *runtime,
                           Context ctx,
                           intptr_t sstart,
                           intptr_t send,
                           PhysicalRegion rz[2],
                           PhysicalRegion rs[1])
{
  RegionAccessor<AOS_ZONES, intptr_t> accessor_znump =
    rz[0].get_field_accessor(FIELD_ZNUMP).typeify<intptr_t>().convert<AOS_ZONES>();
  RegionAccessor<AOS_ZONES, double> accessor_zdl =
    rz[1].get_field_accessor(FIELD_ZDL).typeify<double>().convert<AOS_ZONES>();

  RegionAccessor<AOS_SIDES, ptr_t> accessor_mapsz =
    rs[0].get_field_accessor(FIELD_MAPSZ).typeify<ptr_t>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, double> accessor_sarea =
    rs[0].get_field_accessor(FIELD_SAREA).typeify<double>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, double> accessor_elen =
    rs[0].get_field_accessor(FIELD_ELEN).typeify<double>().convert<AOS_SIDES>();

  for (intptr_t s = sstart; s < send; s++) {
    ptr_t z = accessor_mapsz.read(s);

    intptr_t znump = accessor_znump.read(z);

    double area = accessor_sarea.read(s);
    double base = accessor_elen.read(s);
    double fac;
    if (znump == 3) {
      fac = 3.0;
    } else {
      fac = 4.0;
    }
    double sdl = fac * area / base;
    accessor_zdl.write(z, fmin(accessor_zdl.read(z), sdl));
  }
}

void foreign_calc_rho_half(HighLevelRuntime *runtime,
                           Context ctx,
                           intptr_t zstart,
                           intptr_t zend,
                           PhysicalRegion rz[2])
{
  RegionAccessor<AOS_ZONES, double> accessor_zvolp =
    rz[0].get_field_accessor(FIELD_ZVOLP).typeify<double>().convert<AOS_ZONES>();
  RegionAccessor<AOS_ZONES, double> accessor_zm =
    rz[0].get_field_accessor(FIELD_ZM).typeify<double>().convert<AOS_ZONES>();
  RegionAccessor<AOS_ZONES, double> accessor_zrp =
    rz[1].get_field_accessor(FIELD_ZRP).typeify<double>().convert<AOS_ZONES>();

  for (intptr_t z = zstart; z < zend; z++) {
    accessor_zrp.write(z, accessor_zm.read(z) / accessor_zvolp.read(z));
  }
}

void foreign_sum_point_mass(HighLevelRuntime *runtime,
                            Context ctx,
                            intptr_t sstart,
                            intptr_t send,
                            PhysicalRegion rz[1],
                            PhysicalRegion rpp[1],
                            PhysicalRegion rpg[1],
                            PhysicalRegion rs[1])
{
  RegionAccessor<AOS_ZONES, double> accessor_zareap =
    rz[0].get_field_accessor(FIELD_ZAREAP).typeify<double>().convert<AOS_ZONES>();
  RegionAccessor<AOS_ZONES, double> accessor_zrp =
    rz[0].get_field_accessor(FIELD_ZRP).typeify<double>().convert<AOS_ZONES>();

  RegionAccessor<AOS_REDUCE, double> accessor_rpp_pmaswt =
    rpp[0].get_accessor().typeify<double>().convert<AOS_REDUCE>();

  RegionAccessor<AOS_REDUCE, double> accessor_rpg_pmaswt =
    rpg[0].get_accessor().typeify<double>().convert<AOS_REDUCE>();

  RegionAccessor<AOS_SIDES, ptr_t> accessor_mapsz =
    rs[0].get_field_accessor(FIELD_MAPSZ).typeify<ptr_t>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, ptr_t> accessor_mapsp1_pointer =
    rs[0].get_field_accessor(FIELD_MAPSP1_POINTER).typeify<ptr_t>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, uint32_t> accessor_mapsp1_region =
    rs[0].get_field_accessor(FIELD_MAPSP1_REGION).typeify<uint32_t>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, ptr_t> accessor_mapss3 =
    rs[0].get_field_accessor(FIELD_MAPSS3).typeify<ptr_t>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, double> accessor_smf =
    rs[0].get_field_accessor(FIELD_SMF).typeify<double>().convert<AOS_SIDES>();

  for (intptr_t s = sstart; s < send; s++) {
    ptr_t z = accessor_mapsz.read(s);
    ptr_t p1_pointer = accessor_mapsp1_pointer.read(s);
    uint32_t p1_region = accessor_mapsp1_region.read(s);
    ptr_t s3 = accessor_mapss3.read(s);

    double m = accessor_zrp.read(z) * accessor_zareap.read(z) *
      0.5 * (accessor_smf.read(s) + accessor_smf.read(s3));

    switch (p1_region) {
    case 1:
      accessor_rpp_pmaswt.reduce<reduction_plus_double>(p1_pointer, m);
      break;
    case 2:
      accessor_rpg_pmaswt.reduce<reduction_plus_double>(p1_pointer, m);
      break;
    }
  }
}

void foreign_calc_state_at_half(HighLevelRuntime *runtime,
                                Context ctx,
                                double gamma,
                                double ssmin,
                                double dt,
                                intptr_t zstart,
                                intptr_t zend,
                                PhysicalRegion rz[2])
{
  RegionAccessor<AOS_ZONES, double> accessor_zvol0 =
    rz[0].get_field_accessor(FIELD_ZVOL0).typeify<double>().convert<AOS_ZONES>();
  RegionAccessor<AOS_ZONES, double> accessor_zvolp =
    rz[0].get_field_accessor(FIELD_ZVOLP).typeify<double>().convert<AOS_ZONES>();
  RegionAccessor<AOS_ZONES, double> accessor_zm =
    rz[0].get_field_accessor(FIELD_ZM).typeify<double>().convert<AOS_ZONES>();
  RegionAccessor<AOS_ZONES, double> accessor_zr =
    rz[0].get_field_accessor(FIELD_ZR).typeify<double>().convert<AOS_ZONES>();
  RegionAccessor<AOS_ZONES, double> accessor_ze =
    rz[0].get_field_accessor(FIELD_ZE).typeify<double>().convert<AOS_ZONES>();
  RegionAccessor<AOS_ZONES, double> accessor_zwrate =
    rz[0].get_field_accessor(FIELD_ZWRATE).typeify<double>().convert<AOS_ZONES>();
  RegionAccessor<AOS_ZONES, double> accessor_zp =
    rz[1].get_field_accessor(FIELD_ZP).typeify<double>().convert<AOS_ZONES>();
  RegionAccessor<AOS_ZONES, double> accessor_zss =
    rz[1].get_field_accessor(FIELD_ZSS).typeify<double>().convert<AOS_ZONES>();

  double gm1 = gamma - 1.0;
  double ss2 = fmax(ssmin * ssmin, 1e-99);
  double dth = 0.5 * dt;

  for (intptr_t z = zstart; z < zend; z++) {
    double zm = accessor_zm.read(z);
    double zr = accessor_zr.read(z);
    double ze = accessor_ze.read(z);
    double zvol0 = accessor_zvol0.read(z);
    double zvolp = accessor_zvolp.read(z);
    double zwrate = accessor_zwrate.read(z);

    double rx = zr;
    double ex = fmax(ze, 0.0);
    double px = gm1 * rx * ex;
    double prex = gm1 * ex;
    double perx = gm1 * rx;
    double csqd = fmax(ss2, prex + perx * px / (rx * rx));
    double z0per = perx;
    double zss = sqrt(csqd);
    accessor_zss.write(z, zss);

    double zminv = 1.0 / zm;
    double dv = (zvolp - zvol0) * zminv;
    double bulk = zr * zss * zss;
    double denom = 1.0 + 0.5 * z0per * dv;
    double src = zwrate * dth * zminv;
    accessor_zp.write(z, px + (z0per * src - zr * bulk * dv) / denom);
  }
}

void foreign_calc_force_pgas(HighLevelRuntime *runtime,
                             Context ctx,
                             intptr_t sstart,
                             intptr_t send,
                             PhysicalRegion rz[1],
                             PhysicalRegion rs[2])
{
  RegionAccessor<AOS_ZONES, double> accessor_zp =
    rz[0].get_field_accessor(FIELD_ZP).typeify<double>().convert<AOS_ZONES>();

  RegionAccessor<AOS_SIDES, ptr_t> accessor_mapsz =
    rs[0].get_field_accessor(FIELD_MAPSZ).typeify<ptr_t>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, double> accessor_ssurfp_x =
    rs[0].get_field_accessor(FIELD_SSURFP_X).typeify<double>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, double> accessor_ssurfp_y =
    rs[0].get_field_accessor(FIELD_SSURFP_Y).typeify<double>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, double> accessor_sfp_x =
    rs[1].get_field_accessor(FIELD_SFP_X).typeify<double>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, double> accessor_sfp_y =
    rs[1].get_field_accessor(FIELD_SFP_Y).typeify<double>().convert<AOS_SIDES>();

  for (intptr_t s = sstart; s < send; s++) {
    ptr_t z = accessor_mapsz.read(s);

    vec2 ssurfp;
    ssurfp.x = accessor_ssurfp_x.read(s);
    ssurfp.y = accessor_ssurfp_y.read(s);

    double zp = accessor_zp.read(z);
    vec2 sfx = scale(ssurfp, -zp);
    accessor_sfp_x.write(s, sfx.x);
    accessor_sfp_y.write(s, sfx.y);
  }
}

void foreign_calc_force_tts(HighLevelRuntime *runtime,
                            Context ctx,
                            double alfa,
                            double ssmin,
                            intptr_t sstart,
                            intptr_t send,
                            PhysicalRegion rz[1],
                            PhysicalRegion rs[2])
{
  RegionAccessor<AOS_ZONES, double> accessor_zareap =
    rz[0].get_field_accessor(FIELD_ZAREAP).typeify<double>().convert<AOS_ZONES>();
  RegionAccessor<AOS_ZONES, double> accessor_zrp =
    rz[0].get_field_accessor(FIELD_ZRP).typeify<double>().convert<AOS_ZONES>();
  RegionAccessor<AOS_ZONES, double> accessor_zss =
    rz[0].get_field_accessor(FIELD_ZSS).typeify<double>().convert<AOS_ZONES>();

  RegionAccessor<AOS_SIDES, ptr_t> accessor_mapsz =
    rs[0].get_field_accessor(FIELD_MAPSZ).typeify<ptr_t>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, double> accessor_sareap =
    rs[0].get_field_accessor(FIELD_SAREAP).typeify<double>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, double> accessor_smf =
    rs[0].get_field_accessor(FIELD_SMF).typeify<double>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, double> accessor_ssurfp_x =
    rs[0].get_field_accessor(FIELD_SSURFP_X).typeify<double>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, double> accessor_ssurfp_y =
    rs[0].get_field_accessor(FIELD_SSURFP_Y).typeify<double>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, double> accessor_sft_x =
    rs[1].get_field_accessor(FIELD_SFT_X).typeify<double>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, double> accessor_sft_y =
    rs[1].get_field_accessor(FIELD_SFT_Y).typeify<double>().convert<AOS_SIDES>();

  for (intptr_t s = sstart; s < send; s++) {
    ptr_t z = accessor_mapsz.read(s);

    double zareap = accessor_zareap.read(z);
    double zrp = accessor_zrp.read(z);
    double zss = accessor_zss.read(z);
    double sareap = accessor_sareap.read(s);
    double smf = accessor_smf.read(s);
    vec2 ssurfp;
    ssurfp.x = accessor_ssurfp_x.read(s);
    ssurfp.y = accessor_ssurfp_y.read(s);

    double svfacinv = zareap / sareap;
    double srho = zrp * smf * svfacinv;
    double sstmp = fmax(zss, ssmin);
    sstmp = alfa * sstmp * sstmp;
    double sdp = sstmp * (srho - zrp);
    vec2 sqq = scale(ssurfp, -sdp);
    accessor_sft_x.write(s, sqq.x);
    accessor_sft_y.write(s, sqq.y);
  }
}

void foreign_sum_point_force(HighLevelRuntime *runtime,
                             Context ctx,
                             intptr_t sstart,
                             intptr_t send,
                             PhysicalRegion rpp[2],
                             PhysicalRegion rpg[2],
                             PhysicalRegion rs[1])
{
  RegionAccessor<AOS_REDUCE, double> accessor_rpp_pf_y =
    rpp[0].get_accessor().typeify<double>().convert<AOS_REDUCE>();
  RegionAccessor<AOS_REDUCE, double> accessor_rpp_pf_x =
    rpp[1].get_accessor().typeify<double>().convert<AOS_REDUCE>();

  RegionAccessor<AOS_REDUCE, double> accessor_rpg_pf_y =
    rpg[0].get_accessor().typeify<double>().convert<AOS_REDUCE>();
  RegionAccessor<AOS_REDUCE, double> accessor_rpg_pf_x =
    rpg[1].get_accessor().typeify<double>().convert<AOS_REDUCE>();

  RegionAccessor<AOS_SIDES, ptr_t> accessor_mapsz =
    rs[0].get_field_accessor(FIELD_MAPSZ).typeify<ptr_t>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, ptr_t> accessor_mapsp1_pointer =
    rs[0].get_field_accessor(FIELD_MAPSP1_POINTER).typeify<ptr_t>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, uint32_t> accessor_mapsp1_region =
    rs[0].get_field_accessor(FIELD_MAPSP1_REGION).typeify<uint32_t>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, ptr_t> accessor_mapss3 =
    rs[0].get_field_accessor(FIELD_MAPSS3).typeify<ptr_t>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, double> accessor_sfp_x =
    rs[0].get_field_accessor(FIELD_SFP_X).typeify<double>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, double> accessor_sfp_y =
    rs[0].get_field_accessor(FIELD_SFP_Y).typeify<double>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, double> accessor_sfq_x =
    rs[0].get_field_accessor(FIELD_SFQ_X).typeify<double>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, double> accessor_sfq_y =
    rs[0].get_field_accessor(FIELD_SFQ_Y).typeify<double>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, double> accessor_sft_x =
    rs[0].get_field_accessor(FIELD_SFT_X).typeify<double>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, double> accessor_sft_y =
    rs[0].get_field_accessor(FIELD_SFT_Y).typeify<double>().convert<AOS_SIDES>();

  for (intptr_t s = sstart; s < send; s++) {
    ptr_t z = accessor_mapsz.read(s);
    ptr_t p1_pointer = accessor_mapsp1_pointer.read(s);
    uint32_t p1_region = accessor_mapsp1_region.read(s);
    ptr_t s3 = accessor_mapss3.read(s);

    vec2 s_sfp;
    s_sfp.x = accessor_sfp_x.read(s);
    s_sfp.y = accessor_sfp_y.read(s);

    vec2 s3_sfp;
    s3_sfp.x = accessor_sfp_x.read(s3);
    s3_sfp.y = accessor_sfp_y.read(s3);

    vec2 s_sfq;
    s_sfq.x = accessor_sfq_x.read(s);
    s_sfq.y = accessor_sfq_y.read(s);

    vec2 s3_sfq;
    s3_sfq.x = accessor_sfq_x.read(s3);
    s3_sfq.y = accessor_sfq_y.read(s3);

    vec2 s_sft;
    s_sft.x = accessor_sft_x.read(s);
    s_sft.y = accessor_sft_y.read(s);

    vec2 s3_sft;
    s3_sft.x = accessor_sft_x.read(s3);
    s3_sft.y = accessor_sft_y.read(s3);

    vec2 f = sub(add(s_sfp, add(s_sfq, s_sft)),
                 add(s3_sfp, add(s3_sfq, s3_sft)));

    switch (p1_region) {
    case 1:
      accessor_rpp_pf_x.reduce<reduction_plus_double>(p1_pointer, f.x);
      accessor_rpp_pf_y.reduce<reduction_plus_double>(p1_pointer, f.y);
      break;
    case 2:
      accessor_rpg_pf_x.reduce<reduction_plus_double>(p1_pointer, f.x);
      accessor_rpg_pf_y.reduce<reduction_plus_double>(p1_pointer, f.y);
      break;
    }
  }
}

void foreign_calc_centers_full(HighLevelRuntime *runtime,
                               Context ctx,
                               intptr_t sstart,
                               intptr_t send,
                               PhysicalRegion rz[2],
                               PhysicalRegion rpp[1],
                               PhysicalRegion rpg[1],
                               PhysicalRegion rs[2])
{
  RegionAccessor<AOS_ZONES, intptr_t> accessor_znump =
    rz[0].get_field_accessor(FIELD_ZNUMP).typeify<intptr_t>().convert<AOS_ZONES>();
  RegionAccessor<AOS_ZONES, double> accessor_zx_x =
    rz[1].get_field_accessor(FIELD_ZX_X).typeify<double>().convert<AOS_ZONES>();
  RegionAccessor<AOS_ZONES, double> accessor_zx_y =
    rz[1].get_field_accessor(FIELD_ZX_Y).typeify<double>().convert<AOS_ZONES>();

  RegionAccessor<AOS_POINTS, double> accessor_rpp_px_x =
    rpp[0].get_field_accessor(FIELD_PX_X).typeify<double>().convert<AOS_POINTS>();
  RegionAccessor<AOS_POINTS, double> accessor_rpp_px_y =
    rpp[0].get_field_accessor(FIELD_PX_Y).typeify<double>().convert<AOS_POINTS>();

  RegionAccessor<AOS_POINTS, double> accessor_rpg_px_x =
    rpg[0].get_field_accessor(FIELD_PX_X).typeify<double>().convert<AOS_POINTS>();
  RegionAccessor<AOS_POINTS, double> accessor_rpg_px_y =
    rpg[0].get_field_accessor(FIELD_PX_Y).typeify<double>().convert<AOS_POINTS>();

  RegionAccessor<AOS_SIDES, ptr_t> accessor_mapsz =
    rs[0].get_field_accessor(FIELD_MAPSZ).typeify<ptr_t>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, ptr_t> accessor_mapsp1_pointer =
    rs[0].get_field_accessor(FIELD_MAPSP1_POINTER).typeify<ptr_t>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, uint32_t> accessor_mapsp1_region =
    rs[0].get_field_accessor(FIELD_MAPSP1_REGION).typeify<uint32_t>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, ptr_t> accessor_mapsp2_pointer =
    rs[0].get_field_accessor(FIELD_MAPSP2_POINTER).typeify<ptr_t>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, uint32_t> accessor_mapsp2_region =
    rs[0].get_field_accessor(FIELD_MAPSP2_REGION).typeify<uint32_t>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, double> accessor_ex_x =
    rs[1].get_field_accessor(FIELD_EX_X).typeify<double>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, double> accessor_ex_y =
    rs[1].get_field_accessor(FIELD_EX_Y).typeify<double>().convert<AOS_SIDES>();

  for (intptr_t s = sstart; s < send; s++) {
    ptr_t z = accessor_mapsz.read(s);
    ptr_t p1_pointer = accessor_mapsp1_pointer.read(s);
    uint32_t p1_region = accessor_mapsp1_region.read(s);
    ptr_t p2_pointer = accessor_mapsp2_pointer.read(s);
    uint32_t p2_region = accessor_mapsp2_region.read(s);

    vec2 p1_px;
    switch (p1_region) {
    case 1:
      p1_px.x = accessor_rpp_px_x.read(p1_pointer);
      p1_px.y = accessor_rpp_px_y.read(p1_pointer);
      break;
    case 2:
      p1_px.x = accessor_rpg_px_x.read(p1_pointer);
      p1_px.y = accessor_rpg_px_y.read(p1_pointer);
      break;
    }

    vec2 p2_px;
    switch (p2_region) {
    case 1:
      p2_px.x = accessor_rpp_px_x.read(p2_pointer);
      p2_px.y = accessor_rpp_px_y.read(p2_pointer);
      break;
    case 2:
      p2_px.x = accessor_rpg_px_x.read(p2_pointer);
      p2_px.y = accessor_rpg_px_y.read(p2_pointer);
      break;
    }

    vec2 ex = scale(add(p1_px, p2_px), 0.5);
    accessor_ex_x.write(s, ex.x);
    accessor_ex_y.write(s, ex.y);

    double znump = static_cast<double>(accessor_znump.read(z));
    accessor_zx_x.write(z, accessor_zx_x.read(z) + p1_px.x/znump);
    accessor_zx_y.write(z, accessor_zx_y.read(z) + p1_px.y/znump);
  }
}

void foreign_calc_volumes_full(HighLevelRuntime *runtime,
                               Context ctx,
                               intptr_t sstart,
                               intptr_t send,
                               PhysicalRegion rz[2],
                               PhysicalRegion rpp[1],
                               PhysicalRegion rpg[1],
                               PhysicalRegion rs[2])
{
  RegionAccessor<AOS_ZONES, double> accessor_zx_x =
    rz[0].get_field_accessor(FIELD_ZX_X).typeify<double>().convert<AOS_ZONES>();
  RegionAccessor<AOS_ZONES, double> accessor_zx_y =
    rz[0].get_field_accessor(FIELD_ZX_Y).typeify<double>().convert<AOS_ZONES>();
  RegionAccessor<AOS_ZONES, double> accessor_zarea =
    rz[1].get_field_accessor(FIELD_ZAREA).typeify<double>().convert<AOS_ZONES>();
  RegionAccessor<AOS_ZONES, double> accessor_zvol =
    rz[1].get_field_accessor(FIELD_ZVOL).typeify<double>().convert<AOS_ZONES>();

  RegionAccessor<AOS_POINTS, double> accessor_rpp_px_x =
    rpp[0].get_field_accessor(FIELD_PX_X).typeify<double>().convert<AOS_POINTS>();
  RegionAccessor<AOS_POINTS, double> accessor_rpp_px_y =
    rpp[0].get_field_accessor(FIELD_PX_Y).typeify<double>().convert<AOS_POINTS>();

  RegionAccessor<AOS_POINTS, double> accessor_rpg_px_x =
    rpg[0].get_field_accessor(FIELD_PX_X).typeify<double>().convert<AOS_POINTS>();
  RegionAccessor<AOS_POINTS, double> accessor_rpg_px_y =
    rpg[0].get_field_accessor(FIELD_PX_Y).typeify<double>().convert<AOS_POINTS>();

  RegionAccessor<AOS_SIDES, ptr_t> accessor_mapsz =
    rs[0].get_field_accessor(FIELD_MAPSZ).typeify<ptr_t>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, ptr_t> accessor_mapsp1_pointer =
    rs[0].get_field_accessor(FIELD_MAPSP1_POINTER).typeify<ptr_t>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, uint32_t> accessor_mapsp1_region =
    rs[0].get_field_accessor(FIELD_MAPSP1_REGION).typeify<uint32_t>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, ptr_t> accessor_mapsp2_pointer =
    rs[0].get_field_accessor(FIELD_MAPSP2_POINTER).typeify<ptr_t>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, uint32_t> accessor_mapsp2_region =
    rs[0].get_field_accessor(FIELD_MAPSP2_REGION).typeify<uint32_t>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, double> accessor_sarea =
    rs[1].get_field_accessor(FIELD_SAREA).typeify<double>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, double> accessor_svol =
    rs[1].get_field_accessor(FIELD_SVOL).typeify<double>().convert<AOS_SIDES>();

  for (intptr_t s = sstart; s < send; s++) {
    ptr_t z = accessor_mapsz.read(s);
    ptr_t p1_pointer = accessor_mapsp1_pointer.read(s);
    uint32_t p1_region = accessor_mapsp1_region.read(s);
    ptr_t p2_pointer = accessor_mapsp2_pointer.read(s);
    uint32_t p2_region = accessor_mapsp2_region.read(s);

    vec2 zx;
    zx.x = accessor_zx_x.read(z);
    zx.y = accessor_zx_y.read(z);

    vec2 p1_px;
    switch (p1_region) {
    case 1:
      p1_px.x = accessor_rpp_px_x.read(p1_pointer);
      p1_px.y = accessor_rpp_px_y.read(p1_pointer);
      break;
    case 2:
      p1_px.x = accessor_rpg_px_x.read(p1_pointer);
      p1_px.y = accessor_rpg_px_y.read(p1_pointer);
      break;
    }

    vec2 p2_px;
    switch (p2_region) {
    case 1:
      p2_px.x = accessor_rpp_px_x.read(p2_pointer);
      p2_px.y = accessor_rpp_px_y.read(p2_pointer);
      break;
    case 2:
      p2_px.x = accessor_rpg_px_x.read(p2_pointer);
      p2_px.y = accessor_rpg_px_y.read(p2_pointer);
      break;
    }

    double sa = 0.5 * cross(sub(p2_px, p1_px), sub(zx, p1_px));
    double sv = (1.0 / 3.0) * sa * (p1_px.x + p2_px.x + zx.x);

    accessor_sarea.write(s, sa);
    accessor_svol.write(s, sv);
    accessor_zarea.write(z, accessor_zarea.read(z) + sa);
    accessor_zvol.write(z, accessor_zvol.read(z) + sv);
    assert(sv > 0.0);
  }
}

void foreign_calc_work(HighLevelRuntime *runtime,
                       Context ctx,
                       double dt,
                       intptr_t sstart,
                       intptr_t send,
                       PhysicalRegion rz[1],
                       PhysicalRegion rpp[1],
                       PhysicalRegion rpg[1],
                       PhysicalRegion rs[1])
{
  RegionAccessor<AOS_ZONES, double> accessor_zw =
    rz[0].get_field_accessor(FIELD_ZW).typeify<double>().convert<AOS_ZONES>();
  RegionAccessor<AOS_ZONES, double> accessor_zetot =
    rz[0].get_field_accessor(FIELD_ZETOT).typeify<double>().convert<AOS_ZONES>();

  RegionAccessor<AOS_POINTS, double> accessor_rpp_pxp_x =
    rpp[0].get_field_accessor(FIELD_PXP_X).typeify<double>().convert<AOS_POINTS>();
  RegionAccessor<AOS_POINTS, double> accessor_rpp_pxp_y =
    rpp[0].get_field_accessor(FIELD_PXP_Y).typeify<double>().convert<AOS_POINTS>();
  RegionAccessor<AOS_POINTS, double> accessor_rpp_pu0_x =
    rpp[0].get_field_accessor(FIELD_PU0_X).typeify<double>().convert<AOS_POINTS>();
  RegionAccessor<AOS_POINTS, double> accessor_rpp_pu0_y =
    rpp[0].get_field_accessor(FIELD_PU0_Y).typeify<double>().convert<AOS_POINTS>();
  RegionAccessor<AOS_POINTS, double> accessor_rpp_pu_x =
    rpp[0].get_field_accessor(FIELD_PU_X).typeify<double>().convert<AOS_POINTS>();
  RegionAccessor<AOS_POINTS, double> accessor_rpp_pu_y =
    rpp[0].get_field_accessor(FIELD_PU_Y).typeify<double>().convert<AOS_POINTS>();

  RegionAccessor<AOS_POINTS, double> accessor_rpg_pxp_x =
    rpg[0].get_field_accessor(FIELD_PXP_X).typeify<double>().convert<AOS_POINTS>();
  RegionAccessor<AOS_POINTS, double> accessor_rpg_pxp_y =
    rpg[0].get_field_accessor(FIELD_PXP_Y).typeify<double>().convert<AOS_POINTS>();
  RegionAccessor<AOS_POINTS, double> accessor_rpg_pu0_x =
    rpg[0].get_field_accessor(FIELD_PU0_X).typeify<double>().convert<AOS_POINTS>();
  RegionAccessor<AOS_POINTS, double> accessor_rpg_pu0_y =
    rpg[0].get_field_accessor(FIELD_PU0_Y).typeify<double>().convert<AOS_POINTS>();
  RegionAccessor<AOS_POINTS, double> accessor_rpg_pu_x =
    rpg[0].get_field_accessor(FIELD_PU_X).typeify<double>().convert<AOS_POINTS>();
  RegionAccessor<AOS_POINTS, double> accessor_rpg_pu_y =
    rpg[0].get_field_accessor(FIELD_PU_Y).typeify<double>().convert<AOS_POINTS>();

  RegionAccessor<AOS_SIDES, ptr_t> accessor_mapsz =
    rs[0].get_field_accessor(FIELD_MAPSZ).typeify<ptr_t>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, ptr_t> accessor_mapsp1_pointer =
    rs[0].get_field_accessor(FIELD_MAPSP1_POINTER).typeify<ptr_t>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, uint32_t> accessor_mapsp1_region =
    rs[0].get_field_accessor(FIELD_MAPSP1_REGION).typeify<uint32_t>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, ptr_t> accessor_mapsp2_pointer =
    rs[0].get_field_accessor(FIELD_MAPSP2_POINTER).typeify<ptr_t>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, uint32_t> accessor_mapsp2_region =
    rs[0].get_field_accessor(FIELD_MAPSP2_REGION).typeify<uint32_t>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, double> accessor_sfp_x =
    rs[0].get_field_accessor(FIELD_SFP_X).typeify<double>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, double> accessor_sfp_y =
    rs[0].get_field_accessor(FIELD_SFP_Y).typeify<double>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, double> accessor_sfq_x =
    rs[0].get_field_accessor(FIELD_SFQ_X).typeify<double>().convert<AOS_SIDES>();
  RegionAccessor<AOS_SIDES, double> accessor_sfq_y =
    rs[0].get_field_accessor(FIELD_SFQ_Y).typeify<double>().convert<AOS_SIDES>();

  for (intptr_t s = sstart; s < send; s++) {
    ptr_t z = accessor_mapsz.read(s);
    ptr_t p1_pointer = accessor_mapsp1_pointer.read(s);
    uint32_t p1_region = accessor_mapsp1_region.read(s);
    ptr_t p2_pointer = accessor_mapsp2_pointer.read(s);
    uint32_t p2_region = accessor_mapsp2_region.read(s);

    vec2 sfp;
    sfp.x = accessor_sfp_x.read(s);
    sfp.y = accessor_sfp_y.read(s);

    vec2 sfq;
    sfq.x = accessor_sfq_x.read(s);
    sfq.y = accessor_sfq_y.read(s);

    vec2 p1_pu0;
    switch (p1_region) {
    case 1:
      p1_pu0.x = accessor_rpp_pu0_x.read(p1_pointer);
      p1_pu0.y = accessor_rpp_pu0_y.read(p1_pointer);
      break;
    case 2:
      p1_pu0.x = accessor_rpg_pu0_x.read(p1_pointer);
      p1_pu0.y = accessor_rpg_pu0_y.read(p1_pointer);
      break;
    }

    vec2 p1_pu;
    switch (p1_region) {
    case 1:
      p1_pu.x = accessor_rpp_pu_x.read(p1_pointer);
      p1_pu.y = accessor_rpp_pu_y.read(p1_pointer);
      break;
    case 2:
      p1_pu.x = accessor_rpg_pu_x.read(p1_pointer);
      p1_pu.y = accessor_rpg_pu_y.read(p1_pointer);
      break;
    }

    vec2 p2_pu0;
    switch (p2_region) {
    case 1:
      p2_pu0.x = accessor_rpp_pu0_x.read(p2_pointer);
      p2_pu0.y = accessor_rpp_pu0_y.read(p2_pointer);
      break;
    case 2:
      p2_pu0.x = accessor_rpg_pu0_x.read(p2_pointer);
      p2_pu0.y = accessor_rpg_pu0_y.read(p2_pointer);
      break;
    }

    vec2 p2_pu;
    switch (p2_region) {
    case 1:
      p2_pu.x = accessor_rpp_pu_x.read(p2_pointer);
      p2_pu.y = accessor_rpp_pu_y.read(p2_pointer);
      break;
    case 2:
      p2_pu.x = accessor_rpg_pu_x.read(p2_pointer);
      p2_pu.y = accessor_rpg_pu_y.read(p2_pointer);
      break;
    }

    double p1_pxp_x;
    switch (p1_region) {
    case 1:
      p1_pxp_x = accessor_rpp_pxp_x.read(p1_pointer);
      break;
    case 2:
      p1_pxp_x = accessor_rpg_pxp_x.read(p1_pointer);
      break;
    }

    double p2_pxp_x;
    switch (p2_region) {
    case 1:
      p2_pxp_x = accessor_rpp_pxp_x.read(p2_pointer);
      break;
    case 2:
      p2_pxp_x = accessor_rpg_pxp_x.read(p2_pointer);
      break;
    }

    vec2 sftot = add(sfp, sfq);
    double sd1 = dot(sftot, add(p1_pu0, p1_pu));
    double sd2 = dot(scale(sftot, -1.0), add(p2_pu0, p2_pu));
    double dwork = -0.5 * dt * (sd1 * p1_pxp_x + sd2 * p2_pxp_x);

    accessor_zetot.write(z, accessor_zetot.read(z) + dwork);
    accessor_zw.write(z, accessor_zw.read(z) + dwork);
  }
}

///
/// Vectors
///

double length(vec2 a)
{
  return sqrt(dot(a, a));
}

///
/// Mapper
///

class PennantMapper : public DefaultMapper
{
public:
  PennantMapper(Machine *machine, HighLevelRuntime *rt, Processor local);
  virtual void select_task_options(Task *task);
  virtual bool map_task(Task *task);
  virtual bool map_inline(Inline *inline_operation);
  virtual void notify_mapping_failed(const Mappable *mappable);
private:
  Color get_task_color_by_region(Task *task, LogicalRegion region);
private:
  std::map<Processor::Kind, std::vector<Processor> > all_processors;
};

PennantMapper::PennantMapper(Machine *machine, HighLevelRuntime *rt, Processor local)
  : DefaultMapper(machine, rt, local)
{
  const std::set<Processor> &procs = machine->get_all_processors();
  for (std::set<Processor>::const_iterator it = procs.begin();
       it != procs.end(); it++) {
    Processor::Kind kind = machine->get_processor_kind(*it);
    all_processors[kind].push_back(*it);
  }
}

void PennantMapper::select_task_options(Task *task)
{
  switch (task->task_id) {
  case TASK_INIT_POINTERS:
  case TASK_INIT_MESH_ZONES:
  case TASK_INIT_SIDE_FRACS:
  case TASK_INIT_HYDRO:
  case TASK_INIT_RADIAL_VELOCITY:
  case TASK_INIT_STEP_POINTS:
  case TASK_ADV_POS_HALF:
  case TASK_INIT_STEP_ZONES:
  case TASK_CALC_CENTERS:
  case TASK_CALC_VOLUMES:
  case TASK_CALC_SURFACE_VECS:
  case TASK_CALC_EDGE_LEN:
  case TASK_CALC_CHAR_LEN:
  case TASK_CALC_RHO_HALF:
  case TASK_SUM_POINT_MASS:
  case TASK_CALC_STATE_AT_HALF:
  case TASK_CALC_FORCE_PGAS:
  case TASK_CALC_FORCE_TTS:
  case TASK_QCS_ZONE_CENTER_VELOCITY:
  case TASK_QCS_CORNER_DIVERGENCE:
  case TASK_QCS_QCN_FORCE:
  case TASK_QCS_FORCE:
  case TASK_CALC_FORCE_QCS:
  case TASK_SUM_POINT_FORCE:
  case TASK_APPLY_BOUNDARY_CONDITIONS:
  case TASK_CALC_ACCEL:
  case TASK_ADV_POS_FULL:
  case TASK_CALC_CENTERS_FULL:
  case TASK_CALC_VOLUMES_FULL:
  case TASK_CALC_WORK:
  case TASK_CALC_WORK_RATE:
  case TASK_CALC_ENERGY:
  case TASK_CALC_RHO_FULL:
  case TASK_CALC_DT_COURANT:
  case TASK_CALC_DT_VOLUME:
  case TASK_CALC_DT_HYDRO:
    {
      assert(task->regions.size() >= 1);
      LogicalRegion region = task->regions[0].region;
      Color color = get_task_color_by_region(task, region);

      // Task options:
      task->inline_task = false;
      task->spawn_task = false;
      task->map_locally = task->variants->leaf;
      task->profile_task = false;

      // Processor (round robin by piece of graph):
      std::vector<Processor> &procs = all_processors[Processor::LOC_PROC];
      task->target_proc = procs[color % procs.size()];
    }
    return;
  default:
    DefaultMapper::select_task_options(task);
    return;
  }
}

bool PennantMapper::map_task(Task *task)
{
  Memory global_memory = machine_interface.find_global_memory();

  std::vector<RegionRequirement> &regions = task->regions;
  for (std::vector<RegionRequirement>::iterator it = regions.begin();
        it != regions.end(); it++) {
    RegionRequirement &req = *it;

    // Region options:
    req.virtual_map = false;
    req.enable_WAR_optimization = false;
    req.reduction_list = false;
    req.blocking_factor = 1;

    // Place all regions in global memory.
    req.target_ranking.push_back(global_memory);
  }

  return false;
}

bool PennantMapper::map_inline(Inline *inline_operation)
{
  Memory global_memory = machine_interface.find_global_memory();

  RegionRequirement &req = inline_operation->requirement;

  // Region options:
  req.virtual_map = false;
  req.enable_WAR_optimization = false;
  req.reduction_list = false;
  req.blocking_factor = 1;

  // Place all regions in global memory.
  req.target_ranking.push_back(global_memory);

  return false;
}

void PennantMapper::notify_mapping_failed(const Mappable *mappable)
{
  assert(0 && "mapping failed");
}


Color PennantMapper::get_task_color_by_region(Task *task, LogicalRegion region)
{
  Context ctx = dynamic_cast<Context>(task);
  assert(ctx);

  return runtime->get_logical_region_color(ctx, region);
}

void create_mappers(Machine *machine, HighLevelRuntime *runtime, const std::set<Processor> &local_procs)
{
  for (std::set<Processor>::const_iterator it = local_procs.begin();
        it != local_procs.end(); it++)
  {
    runtime->replace_default_mapper(new PennantMapper(machine, runtime, *it), *it);
  }
}

///
/// Main
///

int main(int argc, char **argv)
{
  HighLevelRuntime::set_registration_callback(create_mappers);
  init_pennant_lg();
  HighLevelRuntime::set_top_level_task_id(TASK_TOPLEVEL);

  return HighLevelRuntime::start(argc, argv);
}
