// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_MAILBOX_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_MAILBOX_MANAGER_H_

#include <functional>
#include <map>

#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/gpu_export.h"

typedef signed char GLbyte;

namespace gpu {
namespace gles2 {

class Texture;
class TextureManager;

// Manages resources scoped beyond the context or context group level.
class GPU_EXPORT MailboxManager : public base::RefCounted<MailboxManager> {
 public:
  MailboxManager();

  // Look up the texture definition from the named mailbox.
  Texture* ConsumeTexture(unsigned target, const Mailbox& mailbox);

  // Put the texture into the named mailbox.
  void ProduceTexture(unsigned target,
                      const Mailbox& mailbox,
                      Texture* texture);

  // Destroy any mailbox that reference the given texture.
  void TextureDeleted(Texture* texture);

 private:
  friend class base::RefCounted<MailboxManager>;

  ~MailboxManager();

  struct TargetName {
    TargetName(unsigned target, const Mailbox& mailbox);
    unsigned target;
    Mailbox mailbox;
  };

  static bool TargetNameLess(const TargetName& lhs, const TargetName& rhs);

  // This is a bidirectional map between mailbox and textures. We can have
  // multiple mailboxes per texture, but one texture per mailbox. We keep an
  // iterator in the MailboxToTextureMap to be able to manage changes to
  // the TextureToMailboxMap efficiently.
  typedef std::multimap<Texture*, TargetName> TextureToMailboxMap;
  typedef std::map<TargetName,
                   TextureToMailboxMap::iterator,
                   std::pointer_to_binary_function<const TargetName&,
                                                   const TargetName&,
                                                   bool> > MailboxToTextureMap;

  MailboxToTextureMap mailbox_to_textures_;
  TextureToMailboxMap textures_to_mailboxes_;

  DISALLOW_COPY_AND_ASSIGN(MailboxManager);
};
}  // namespage gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_MAILBOX_MANAGER_H_

