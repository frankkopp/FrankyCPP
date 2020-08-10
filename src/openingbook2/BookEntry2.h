/*
 * MIT License
 *
 * Copyright (c) 2018 Frank Kopp
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#ifndef FRANKYCPP_BOOKENTRY2_H
#define FRANKYCPP_BOOKENTRY2_H

#include <memory>
#include <vector>

#include "types/types.h"

#include <boost/serialization/unordered_map.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/shared_ptr.hpp>

/**
 * An entry in the opening book data structure. Stores a key (e.g. zobrist key)
 * for the position, the current fen, a count of how often a position is in the
 * book and two vectors storing the moves from the position and pointers to the
 * book entry for the corresponding move.
 */
struct BookEntry2 {
  Key key{};
  int counter{0};
  std::vector<Move> moves{};
  std::vector<std::shared_ptr<BookEntry2>> ptrNextPosition{};

  explicit BookEntry2(Key zobrist) : key(zobrist), counter{1} {}

  std::string str();

  // BOOST Serialization
  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive & ar, [[maybe_unused]] const unsigned int version) {
    ar & BOOST_SERIALIZATION_NVP (key);
    ar & BOOST_SERIALIZATION_NVP (counter);
    ar & BOOST_SERIALIZATION_NVP (moves);
    ar & BOOST_SERIALIZATION_NVP (ptrNextPosition);
  }
};



#endif//FRANKYCPP_BOOKENTRY2_H
