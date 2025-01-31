// Copyright (c) 2019-2021 Simons Foundation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0.txt
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Authors: Olivier Parcollet, Nils Wentzell

#include "./test_common.hpp"
#include <nda/mpi.hpp>
#include <fstream>

#include <nda/clef/literals.hpp>
using namespace nda::clef::literals;

using nda::range;

// COPY for itertools to make the test independent from itertools package
std::pair<std::ptrdiff_t, std::ptrdiff_t> chunk_range(std::ptrdiff_t start, std::ptrdiff_t end, long n_chunks, long rank) {
  auto total_size    = end - start;
  auto chunk_size    = total_size / n_chunks;
  auto n_large_nodes = total_size - n_chunks * chunk_size;
  if (rank < n_large_nodes) // larger nodes have size chunk_size + 1
    return {start + rank * (chunk_size + 1), start + (rank + 1) * (chunk_size + 1)};
  else // smaller nodes have size chunk_size
    return {start + n_large_nodes + rank * chunk_size, start + n_large_nodes + (rank + 1) * chunk_size};
}

// --------------------------------------

// test reduce MAX, MIN
TEST(Arrays, MPIReduceMAX) { //NOLINT

  mpi::communicator world;
  using arr_t = nda::array<int, 1>;
  auto r      = world.rank();
  auto s      = world.size();

  arr_t a(7);
  for (int i = 0; i < a.extent(0); ++i) a(i) = (i - r + 2) * (i - r + 2);

  auto b1 = a, b2 = a;
  for (int i = 0; i < 7; ++i) {
    arr_t c(s);
    for (int j = 0; j < c.extent(0); ++j) c(j) = (i - j + 2) * (i - j + 2);
    b1(i) = min_element(c);
    b2(i) = max_element(c);
  }

  arr_t r1 = mpi::reduce(a, world, 0, true, MPI_MIN);
  arr_t r2 = mpi::reduce(a, world, 0, true, MPI_MAX);

  std::cerr << " a = " << r << a << std::endl;
  std::cerr << "r1 = " << r << r1 << std::endl;
  std::cerr << "r2 = " << r << r2 << std::endl;

  EXPECT_ARRAY_EQ(r1, b1);
  EXPECT_ARRAY_EQ(r2, b2);
}

// --------------------------------------

TEST(Arrays, MPI) { //NOLINT

  mpi::communicator world;

  // using arr_t = nda::array<double,2>;
  using arr_t = nda::array<std::complex<double>, 2>;

  arr_t A(7, 3), B, AA;

  auto se = itertools::chunk_range(0, 7, world.size(), world.rank());

  for (int i = 0; i < A.extent(0); ++i)
    for (int j = 0; j < A.extent(1); ++j) A(i, j) = i + 10 * j;

  B       = mpi::scatter(A, world);
  arr_t C = mpi::scatter(A, world);

  std::ofstream out("node" + std::to_string(world.rank()));
  out << "  A = " << A << std::endl;
  out << "  B = " << B << std::endl;
  out << "  C = " << C << std::endl;

  EXPECT_ARRAY_EQ(B, A(range(se.first, se.second), range()));
  EXPECT_ARRAY_NEAR(C, B);

  B *= -1;
  AA() = 0;

  AA = mpi_gather(B, world);
  if (world.rank() == 0) { EXPECT_ARRAY_NEAR(AA, -A); }

  mpi::broadcast(AA, world);
  EXPECT_ARRAY_NEAR(AA, -A);

  AA() = 0;
  AA   = mpi::all_gather(B, world);
  EXPECT_ARRAY_NEAR(AA, -A);

  arr_t r1 = mpi::reduce(A, world);
  if (world.rank() == 0) { EXPECT_ARRAY_NEAR(r1, world.size() * A); }

  arr_t r2 = mpi::all_reduce(A, world);
  EXPECT_ARRAY_NEAR(r2, world.size() * A);
}

// --------------------------------------

TEST(Arrays, MPIReduceCustom) { //NOLINT

  mpi::communicator world;
  using matrix_t = nda::matrix<double>;
  using arr_t = nda::array<matrix_t, 1>;

  arr_t a(7);
  arr_t r_exact(7);
  for (int i = 0; i < a.extent(0); ++i){
    a(i) = matrix_t{4, 4};
    a(i)(k_, l_) << i * (world.rank() + 1) * (k_ + l_);

    r_exact(i) = matrix_t{4, 4};
    r_exact(i)(k_, l_) << i * (world.size() + 1) * world.size() / 2 * (k_ + l_);
  }

  arr_t r = mpi::all_reduce(a, world);

  EXPECT_ARRAY_EQ(r, r_exact);
}

// --------------------------------------

// test transposed matrix broadcast
TEST(Arrays, matrix_transpose_bcast) { //NOLINT

  mpi::communicator world;

  nda::matrix<dcomplex> A  = {{1, 2, 3}, {4, 5, 6}};
  nda::matrix<dcomplex> At = transpose(A);

  nda::matrix<dcomplex> B;
  if (world.rank() == 0) B = At;

  mpi::broadcast(B, world, 0);

  EXPECT_ARRAY_EQ(At, B);
}

// --------------------------------------

// test transposed array broadcast
TEST(Arrays, array_transpose_bcast) { //NOLINT

  mpi::communicator world;

  nda::array<dcomplex, 2> A  = {{1, 2, 3}, {4, 5, 6}};
  nda::array<dcomplex, 2> At = transpose(A);

  nda::array<dcomplex, 2> B(2, 3);
  if (world.rank() == 0) B = At;

  mpi::broadcast(B, world, 0);

  EXPECT_ARRAY_EQ(At, B);
}

MAKE_MAIN_MPI
