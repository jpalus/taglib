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

#include "vorbisfile.h"

#include "tdebug.h"
#include "tpropertymap.h"
#include "tagutils.h"

using namespace TagLib;

class Vorbis::File::FilePrivate
{
public:
  std::unique_ptr<Ogg::XiphComment> comment;
  std::unique_ptr<Properties> properties;
};

namespace TagLib {
  /*!
   * Vorbis headers can be found with one type ID byte and the string "vorbis" in
   * an Ogg stream.  0x03 indicates the comment header.
   */
  static const char vorbisCommentHeaderID[] = { 0x03, 'v', 'o', 'r', 'b', 'i', 's', 0 };
} // namespace TagLib

////////////////////////////////////////////////////////////////////////////////
// static members
////////////////////////////////////////////////////////////////////////////////

bool Vorbis::File::isSupported(IOStream *stream)
{
  // An Ogg Vorbis file has IDs "OggS" and "\x01vorbis" somewhere.

  const ByteVector buffer = Utils::readHeader(stream, bufferSize(), false);
  return (buffer.find("OggS") >= 0 && buffer.find("\x01vorbis") >= 0);
}

////////////////////////////////////////////////////////////////////////////////
// public members
////////////////////////////////////////////////////////////////////////////////

Vorbis::File::File(FileName file, bool readProperties, Properties::ReadStyle) :
  Ogg::File(file),
  d(std::make_unique<FilePrivate>())
{
  if(isOpen())
    read(readProperties);
}

Vorbis::File::File(IOStream *stream, bool readProperties, Properties::ReadStyle) :
  Ogg::File(stream),
  d(std::make_unique<FilePrivate>())
{
  if(isOpen())
    read(readProperties);
}

Vorbis::File::~File() = default;

Ogg::XiphComment *Vorbis::File::tag() const
{
  return d->comment.get();
}

PropertyMap Vorbis::File::properties() const
{
  return d->comment->properties();
}

PropertyMap Vorbis::File::setProperties(const PropertyMap &properties)
{
  return d->comment->setProperties(properties);
}

Vorbis::Properties *Vorbis::File::audioProperties() const
{
  return d->properties.get();
}

bool Vorbis::File::save()
{
  ByteVector v(vorbisCommentHeaderID);

  if(!d->comment)
    d->comment = std::make_unique<Ogg::XiphComment>();
  v.append(d->comment->render());

  setPacket(1, v);

  return Ogg::File::save();
}

////////////////////////////////////////////////////////////////////////////////
// private members
////////////////////////////////////////////////////////////////////////////////

void Vorbis::File::read(bool readProperties)
{
  ByteVector commentHeaderData = packet(1);

  if(commentHeaderData.mid(0, 7) != vorbisCommentHeaderID) {
    debug("Vorbis::File::read() - Could not find the Vorbis comment header.");
    setValid(false);
    return;
  }

  d->comment = std::make_unique<Ogg::XiphComment>(commentHeaderData.mid(7));

  if(readProperties)
    d->properties = std::make_unique<Properties>(this);
}
