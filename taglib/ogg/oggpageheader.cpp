/***************************************************************************
    copyright            : (C) 2002 - 2008 by Scott Wheeler
    email                : wheeler@kde.org
 ***************************************************************************/

/***************************************************************************
 *   This library is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Lesser General Public License version   *
 *   2.1 as published by the Free Software Foundation.                     *
 *                                                                         *
 *   This library is distributed in the hope that it will be useful, but   *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of            *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU     *
 *   Lesser General Public License for more details.                       *
 *                                                                         *
 *   You should have received a copy of the GNU Lesser General Public      *
 *   License along with this library; if not, write to the Free Software   *
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA         *
 *   02110-1301  USA                                                       *
 *                                                                         *
 *   Alternatively, this file is available under the Mozilla Public        *
 *   License Version 1.1.  You may obtain a copy of the License at         *
 *   http://www.mozilla.org/MPL/                                           *
 ***************************************************************************/

#include "oggpageheader.h"

#include <bitset>

#include "tdebug.h"
#include "tstring.h"
#include "oggfile.h"

using namespace TagLib;

class Ogg::PageHeader::PageHeaderPrivate
{
public:
  bool isValid { false };
  List<int> packetSizes;
  bool firstPacketContinued { false };
  bool lastPacketCompleted { false };
  bool firstPageOfStream { false };
  bool lastPageOfStream { false };
  long long absoluteGranularPosition { 0 };
  unsigned int streamSerialNumber { 0 };
  int pageSequenceNumber { -1 };
  int size { 0 };
  int dataSize { 0 };
};

////////////////////////////////////////////////////////////////////////////////
// public members
////////////////////////////////////////////////////////////////////////////////

Ogg::PageHeader::PageHeader(Ogg::File *file, offset_t pageOffset) :
  d(std::make_unique<PageHeaderPrivate>())
{
  if(file && pageOffset >= 0)
    read(file, pageOffset);
}

Ogg::PageHeader::~PageHeader() = default;

bool Ogg::PageHeader::isValid() const
{
  return d->isValid;
}

List<int> Ogg::PageHeader::packetSizes() const
{
  return d->packetSizes;
}

void Ogg::PageHeader::setPacketSizes(const List<int> &sizes)
{
  d->packetSizes = sizes;
}

bool Ogg::PageHeader::firstPacketContinued() const
{
  return d->firstPacketContinued;
}

void Ogg::PageHeader::setFirstPacketContinued(bool continued)
{
  d->firstPacketContinued = continued;
}

bool Ogg::PageHeader::lastPacketCompleted() const
{
  return d->lastPacketCompleted;
}

void Ogg::PageHeader::setLastPacketCompleted(bool completed)
{
  d->lastPacketCompleted = completed;
}

bool Ogg::PageHeader::firstPageOfStream() const
{
  return d->firstPageOfStream;
}

void Ogg::PageHeader::setFirstPageOfStream(bool first)
{
  d->firstPageOfStream = first;
}

bool Ogg::PageHeader::lastPageOfStream() const
{
  return d->lastPageOfStream;
}

void Ogg::PageHeader::setLastPageOfStream(bool last)
{
  d->lastPageOfStream = last;
}

long long Ogg::PageHeader::absoluteGranularPosition() const
{
  return d->absoluteGranularPosition;
}

void Ogg::PageHeader::setAbsoluteGranularPosition(long long agp)
{
  d->absoluteGranularPosition = agp;
}

int Ogg::PageHeader::pageSequenceNumber() const
{
  return d->pageSequenceNumber;
}

void Ogg::PageHeader::setPageSequenceNumber(int sequenceNumber)
{
  d->pageSequenceNumber = sequenceNumber;
}

unsigned int Ogg::PageHeader::streamSerialNumber() const
{
  return d->streamSerialNumber;
}

void Ogg::PageHeader::setStreamSerialNumber(unsigned int n)
{
  d->streamSerialNumber = n;
}

int Ogg::PageHeader::size() const
{
  return d->size;
}

int Ogg::PageHeader::dataSize() const
{
  return d->dataSize;
}

ByteVector Ogg::PageHeader::render() const
{
  ByteVector data;

  // capture pattern

  data.append("OggS");

  // stream structure version

  data.append(static_cast<char>(0));

  // header type flag

  std::bitset<8> flags;
  flags[0] = d->firstPacketContinued;
  flags[1] = d->pageSequenceNumber == 0;
  flags[2] = d->lastPageOfStream;

  data.append(static_cast<char>(flags.to_ulong()));

  // absolute granular position

  data.append(ByteVector::fromLongLong(d->absoluteGranularPosition, false));

  // stream serial number

  data.append(ByteVector::fromUInt(d->streamSerialNumber, false));

  // page sequence number

  data.append(ByteVector::fromUInt(d->pageSequenceNumber, false));

  // checksum -- this is left empty and should be filled in by the Ogg::Page
  // class

  data.append(ByteVector(4, 0));

  // page segment count and page segment table

  ByteVector pageSegments = lacingValues();

  data.append(static_cast<unsigned char>(pageSegments.size()));
  data.append(pageSegments);

  return data;
}

////////////////////////////////////////////////////////////////////////////////
// private members
////////////////////////////////////////////////////////////////////////////////

void Ogg::PageHeader::read(Ogg::File *file, offset_t pageOffset)
{
  file->seek(pageOffset);

  // An Ogg page header is at least 27 bytes, so we'll go ahead and read that
  // much and then get the rest when we're ready for it.

  const ByteVector data = file->readBlock(27);

  // Sanity check -- make sure that we were in fact able to read as much data as
  // we asked for and that the page begins with "OggS".

  if(data.size() != 27 || !data.startsWith("OggS")) {
    debug("Ogg::PageHeader::read() -- error reading page header");
    return;
  }

  const std::bitset<8> flags(data[5]);

  d->firstPacketContinued = flags.test(0);
  d->firstPageOfStream    = flags.test(1);
  d->lastPageOfStream     = flags.test(2);

  d->absoluteGranularPosition = data.toLongLong(6, false);
  d->streamSerialNumber = data.toUInt(14, false);
  d->pageSequenceNumber = data.toUInt(18, false);

  // Byte number 27 is the number of page segments, which is the only variable
  // length portion of the page header.  After reading the number of page
  // segments we'll then read in the corresponding data for this count.

  int pageSegmentCount = static_cast<unsigned char>(data[26]);

  const ByteVector pageSegments = file->readBlock(pageSegmentCount);

  // Another sanity check.

  if(pageSegmentCount < 1 || static_cast<int>(pageSegments.size()) != pageSegmentCount)
    return;

  // The base size of an Ogg page 27 bytes plus the number of lacing values.

  d->size = 27 + pageSegmentCount;

  int packetSize = 0;

  for(int i = 0; i < pageSegmentCount; i++) {
    d->dataSize += static_cast<unsigned char>(pageSegments[i]);
    packetSize += static_cast<unsigned char>(pageSegments[i]);

    if(static_cast<unsigned char>(pageSegments[i]) < 255) {
      d->packetSizes.append(packetSize);
      packetSize = 0;
    }
  }

  if(packetSize > 0) {
    d->packetSizes.append(packetSize);
    d->lastPacketCompleted = false;
  }
  else
    d->lastPacketCompleted = true;

  d->isValid = true;
}

ByteVector Ogg::PageHeader::lacingValues() const
{
  ByteVector data;

  for(auto it = d->packetSizes.cbegin(); it != d->packetSizes.cend(); ++it) {

    // The size of a packet in an Ogg page is indicated by a series of "lacing
    // values" where the sum of the values is the packet size in bytes.  Each of
    // these values is a byte.  A value of less than 255 (0xff) indicates the end
    // of the packet.

    data.resize(data.size() + (*it / 255), '\xff');

    if(it != std::prev(d->packetSizes.cend()) || d->lastPacketCompleted)
      data.append(static_cast<unsigned char>(*it % 255));
  }

  return data;
}
