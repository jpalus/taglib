/***************************************************************************
    copyright            : (C) 2012 by Lukáš Lalinský
    email                : lalinsky@gmail.com

    copyright            : (C) 2002 - 2008 by Scott Wheeler
    email                : wheeler@kde.org
                           (original Vorbis implementation)
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

#include "opusfile.h"

#include "tdebug.h"
#include "tpropertymap.h"
#include "tagutils.h"

using namespace TagLib;
using namespace TagLib::Ogg;

class Opus::File::FilePrivate
{
public:
  std::unique_ptr<Ogg::XiphComment> comment;
  std::unique_ptr<Properties> properties;
};

////////////////////////////////////////////////////////////////////////////////
// static members
////////////////////////////////////////////////////////////////////////////////

bool Ogg::Opus::File::isSupported(IOStream *stream)
{
  // An Opus file has IDs "OggS" and "OpusHead" somewhere.

  const ByteVector buffer = Utils::readHeader(stream, bufferSize(), false);
  return (buffer.find("OggS") >= 0 && buffer.find("OpusHead") >= 0);
}

////////////////////////////////////////////////////////////////////////////////
// public members
////////////////////////////////////////////////////////////////////////////////

Opus::File::File(FileName file, bool readProperties, Properties::ReadStyle) :
  Ogg::File(file),
  d(std::make_unique<FilePrivate>())
{
  if(isOpen())
    read(readProperties);
}

Opus::File::File(IOStream *stream, bool readProperties, Properties::ReadStyle) :
  Ogg::File(stream),
  d(std::make_unique<FilePrivate>())
{
  if(isOpen())
    read(readProperties);
}

Opus::File::~File() = default;

Ogg::XiphComment *Opus::File::tag() const
{
  return d->comment.get();
}

PropertyMap Opus::File::properties() const
{
  return d->comment->properties();
}

PropertyMap Opus::File::setProperties(const PropertyMap &properties)
{
  return d->comment->setProperties(properties);
}

Opus::Properties *Opus::File::audioProperties() const
{
  return d->properties.get();
}

bool Opus::File::save()
{
  if(!d->comment)
    d->comment = std::make_unique<Ogg::XiphComment>();

  setPacket(1, ByteVector("OpusTags", 8) + d->comment->render(false));

  return Ogg::File::save();
}

////////////////////////////////////////////////////////////////////////////////
// private members
////////////////////////////////////////////////////////////////////////////////

void Opus::File::read(bool readProperties)
{
  ByteVector opusHeaderData = packet(0);

  if(!opusHeaderData.startsWith("OpusHead")) {
    setValid(false);
    debug("Opus::File::read() -- invalid Opus identification header");
    return;
  }

  ByteVector commentHeaderData = packet(1);

  if(!commentHeaderData.startsWith("OpusTags")) {
    setValid(false);
    debug("Opus::File::read() -- invalid Opus tags header");
    return;
  }

  d->comment = std::make_unique<Ogg::XiphComment>(commentHeaderData.mid(8));

  if(readProperties)
    d->properties = std::make_unique<Properties>(this);
}
