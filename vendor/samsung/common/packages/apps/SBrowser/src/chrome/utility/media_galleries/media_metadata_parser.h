// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_MEDIA_GALLERIES_MEDIA_METADATA_PARSER_H_
#define CHROME_UTILITY_MEDIA_GALLERIES_MEDIA_METADATA_PARSER_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/common/extensions/api/media_galleries.h"

namespace media {
class DataSource;
}

namespace metadata {

// This class takes a MIME type and data source and parses its metadata. It
// handles audio, video, and images. It delegates its operations to FFMPEG,
// libexif, etc. This class lives and operates on the utility thread of the
// utility process, as we wish to sandbox potentially dangerous operations
// on user-provided data.
class MediaMetadataParser {
 public:
  typedef extensions::api::media_galleries::MediaMetadata MediaMetadata;
  typedef base::Callback<void(scoped_ptr<MediaMetadata>)> MetadataCallback;

  // Does not take ownership of |source|. Caller is responsible for ensuring
  // that |source| outlives this object.
  MediaMetadataParser(media::DataSource* source, const std::string& mime_type);

  ~MediaMetadataParser();

  // |callback| is called on same message loop.
  void Start(const MetadataCallback& callback);

 private:
  void PopulateAudioVideoMetadata();

  media::DataSource* source_;

  MetadataCallback callback_;

  scoped_ptr<MediaMetadata> metadata_;

  DISALLOW_COPY_AND_ASSIGN(MediaMetadataParser);
};

}  // namespace metadata

#endif  // CHROME_UTILITY_MEDIA_GALLERIES_MEDIA_METADATA_PARSER_H_
