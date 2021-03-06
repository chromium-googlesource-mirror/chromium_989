// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_PARSER_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_PARSER_H_
#pragma once

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_vector.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"

class Profile;
namespace base {
class Value;
class DictionaryValue;
}

// Defines data elements of Google Documents API as described in
// http://code.google.com/apis/documents/.
namespace gdata {

// Defines link (URL) of an entity (document, file, feed...). Each entity could
// have more than one link representing it.
class Link {
 public:
  enum LinkType {
    UNKNOWN,
    SELF,
    NEXT,
    PARENT,
    ALTERNATE,
    EDIT,
    EDIT_MEDIA,
    FEED,
    POST,
    BATCH,
    RESUMABLE_EDIT_MEDIA,
    RESUMABLE_CREATE_MEDIA,
    TABLES_FEED,
    WORKSHEET_FEED,
    THUMBNAIL,
  };
  Link();

  // Reads link fields from corresponding fields from |dictionary|.
  bool Parse(const base::DictionaryValue* dictionary);

  // Type of the link.
  LinkType type() const { return type_; }

  // URL of the link.
  const GURL& href() const { return href_; }

  // Title of the link.
  const string16& title() const { return title_; }

  // Link MIME type.
  const std::string& mime_type() const { return mime_type_; }

 private:
  // Converts value of link.rel into LinkType.
  static LinkType GetLinkType(const std::string& rel);

  LinkType type_;
  GURL href_;
  string16 title_;
  std::string mime_type_;

  static const char kHrefField[];
  static const char kRelField[];
  static const char kTitleField[];
  static const char kTypeField[];
};

// Feed links define links (URLs) to special list of entries (i.e. list of
// previous document revisions).
class FeedLink {
 public:
  enum FeedLinkType {
    UNKNOWN,
    ACL,
    REVISIONS,
  };
  FeedLink();

  // Reads link details from corresponding fields from |dictionary|.
  bool Parse(const base::DictionaryValue* dictionary);

  // URL of the feed.
  FeedLinkType type() const { return type_; }

  // MIME type of the feed.
  const GURL& href() const { return href_; }

 private:
  // Converts value of gd$feedLink.rel into FeedLinkType enum.
  static FeedLinkType GetFeedLinkType(const std::string& rel);

  FeedLinkType type_;
  GURL href_;

  static const char kHrefField[];
  static const char kRelField[];
};

class Author {
 public:
  Author();

  // Reads author details from corresponding fields from |dictionary|.
  bool Parse(const base::DictionaryValue* dictionary);

  // Getters.
  const string16& name() const { return name_; }
  const std::string& email() const { return email_; }

 private:
  string16 name_;
  std::string email_;
  static const char kNameField[];
  static const char kEmailField[];
};

// Entry category.
class Category {
 public:
  enum CategoryType {
    UNKNOWN,
    ITEM,
    KIND,
    LABEL,
  };
  Category();

  // Reads category details from corresponding fields from |dictionary|.
  bool Parse(const base::DictionaryValue* dictionary);

  // Category label.
  const string16& label() const { return label_; }

  // Category type.
  CategoryType type() const { return type_; }

  // Category term.
  const std::string& term() const { return term_; }

 private:
  // Converts catory scheme into CategoryType enum. For example,
  // http://schemas.google.com/g/2005#kind => Category::KIND
  CategoryType GetCategoryTypeFromScheme(const std::string& scheme);

  string16 label_;
  CategoryType type_;
  std::string term_;

  static const char kLabelField[];
  static const char kSchemeField[];
  static const char kTermField[];
};

// Base class for feed entries.
class GDataEntry {
 public:
  GDataEntry();
  virtual ~GDataEntry();

  // Returns a link of a given |type| for this entry. If not found, it returns
  // NULL.
  const Link* GetLinkByType(Link::LinkType type) const;

  // Entry update time.
  base::Time updated_time() const { return updated_time_; }

  // Entry ETag.
  const std::string& etag() const { return etag_; }

  // List of entry authors.
  const ScopedVector<Author>& authors() const { return authors_; }

  // List of entry links.
  const ScopedVector<Link>& links() const { return links_; }

  // List of entry categories.
  const ScopedVector<Category>& categories() const { return categories_; }

  // Returns true when time string (in format yyyy-mm-ddThh:mm:ss.dddZ), is
  // successfully parsed and output as |time|.
  static bool GetTimeFromString(const std::string& raw_value, base::Time* time);

 protected:
  // Handler method, invoked when a new category is added to the entry.
  virtual void OnAddCategory(Category* category) {}

  // Parses links from |dictionary|.
  bool ParseLinks(const base::DictionaryValue* dictionary);

  // Parses categories from |dictionary|.
  bool ParseCategories(const base::DictionaryValue* dictionary);

  // Parses authors from |dictionary|.
  bool ParseAuthors(const base::DictionaryValue* dictionary);

  // Returns true when time string (in format yyyy-mm-ddThh:mm:ss.dddZ), is
  // successfully parsed and output as |time|.
  static bool ParseDateTime(const base::DictionaryValue* dict,
                            const std::string& field,
                            base::Time* time);

  std::string etag_;
  ScopedVector<Author> authors_;
  ScopedVector<Link> links_;
  ScopedVector<Category> categories_;
  base::Time updated_time_;

  static const char kTimeParsingDelimiters[];
  static const char kAuthorField[];
  static const char kLinkField[];
  static const char kCategoryField[];
};

// Document feed entry.
class DocumentEntry : public GDataEntry {
 public:
  enum EntryKind {
    UNKNOWN,
    ITEM,
    DOCUMENT,
    SPREADSHEET,
    PRESENTATION,
    FOLDER,
    FILE,
    PDF,
  };
  virtual ~DocumentEntry();
  // Creates document entry from parsed JSON Value.
  static DocumentEntry* CreateFrom(base::Value* value);

  // Document entry resource id.
  const std::string& resource_id() const { return resource_id_; }

  // Document entry id.
  const std::string& id() const { return id_; }

  // Document entry kind.
  EntryKind kind() const { return kind_; }

  // Document entry title.
  const string16& title() const { return title_; }

  // Document entry published time.
  base::Time published_time() const { return published_time_; }

  // List of document feed labels.
  const std::vector<string16>& labels() const { return labels_; }

  // Document entry content URL.
  const GURL& content_url() const { return content_url_; }

  // Document entry MIME type.
  const std::string& content_mime_type() const { return content_mime_type_; }

  // List of document feed links.
  const ScopedVector<FeedLink>& feed_links() const { return feed_links_; }

  // Document feed file name (exists only for kinds FILE and PDF).
  const string16& filename() const { return filename_; }

  // Document feed suggested file name (exists only for kinds FILE and PDF).
  const string16& suggested_filename() const { return suggested_filename_; }

  // Document feed file content MD5 (exists only for kinds FILE and PDF).
  const std::string& file_md5() const { return file_md5_; }

  // Document feed file size (exists only for kinds FILE and PDF).
  int64 file_size() const { return file_size_; }

 private:
  DocumentEntry();

  // Parses and initializes data members from content of |dictionary|. Returns
  // false if parsing fails.
  bool Parse(const base::DictionaryValue* dictionary);

  // Parses properties for file entry from |dictionary|.
  bool ParseFileProperties(const base::DictionaryValue* dictionary);

  // Parses content properties from |dictionary|.
  bool ParseContent(const base::DictionaryValue* dictionary);

  // Parses feed links from |dictionary|.
  bool ParseFeedLinks(const base::DictionaryValue* dictionary);

  // GDataEntry overrides.
  virtual void OnAddCategory(Category* category) OVERRIDE;
  // Converts categories.term into EntryKind enum.
  static EntryKind GetEntryKindFromTerm(const std::string& term);

  std::string resource_id_;
  std::string id_;
  EntryKind kind_;
  string16 title_;
  base::Time published_time_;
  std::vector<string16> labels_;
  GURL content_url_;
  std::string content_mime_type_;
  ScopedVector<FeedLink> feed_links_;
  // Optional fields for files only.
  string16 filename_;
  string16 suggested_filename_;
  std::string file_md5_;
  int64 file_size_;

  static const char kFeedLinkField[];
  static const char kContentField[];
  static const char kSrcField[];
  static const char kTypeField[];
  static const char kFileNameField[];
  static const char kMD5Field[];
  static const char kSizeField[];
  static const char kSuggestedFileNameField[];
  static const char kETagField[];
  static const char kResourceIdField[];
  static const char kIDField[];
  static const char kTitleField[];
  static const char kUpdatedField[];
  static const char kPublishedField[];
};

// Document feed represents a list of entries. The feed is paginated and
// the rest of the feed can be fetched by retrieving the remaining parts of the
// feed from URLs provided by GetNextFeedURL() method.
class DocumentFeed : public GDataEntry {
 public:
  virtual ~DocumentFeed();

  // Creates feed from parsed JSON Value.
  static DocumentFeed* CreateFrom(base::Value* value);

  // Returns true and passes|url| of the next feed if the current entry list
  // does not completed this feed.
  bool GetNextFeedURL(GURL* url);

  // List of document entries.
  const ScopedVector<DocumentEntry>& entries() const { return entries_; }

  // Start index of the document entry list.
  int start_index() const { return start_index_; }

  // Number of items per feed of the document entry list.
  int items_per_page() const { return items_per_page_; }

  // Document entry list title.
  const std::string& title() { return title_; }

 private:
  DocumentFeed();
  // Parses and initializes data members from content of |dictionary|. Returns
  // false if parsing fails.
  bool Parse(const base::DictionaryValue* dictionary);

  ScopedVector<DocumentEntry> entries_;
  int start_index_;
  int items_per_page_;
  std::string title_;

  static const char kETagField[];
  static const char kStartIndexField[];
  static const char kItemsPerPageField[];
  static const char kUpdatedField[];
  static const char kTitleField[];
  static const char kEntryField[];
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_PARSER_H_
