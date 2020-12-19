#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>
#include <windows.h>
#include <wincred.h>
#include "envchain.h"

static const std::string::size_type TARGET_PREFIX_LEN = 9; // == strlen("envchain/")

int
envchain_search_namespaces(envchain_namespace_search_callback callback, void *data)
{
  CREDENTIAL **creds = nullptr;
  DWORD count = 0;
  if (!CredEnumerate("envchain/*", 0, &count, &creds)) {
    const DWORD c = GetLastError();
    if (c == ERROR_NOT_FOUND) {
      return 0;
    } else {
      fprintf(stderr, "%s: CredEnumerate failed with %lu\n", envchain_name, c);
      return 1;
    }
  }

  std::set<std::string> names;
  for (DWORD i = 0; i < count; i++) {
    if (creds[i]->Type == CRED_TYPE_GENERIC) {
      if (creds[i]->CredentialBlob[creds[i]->CredentialBlobSize] != '\0') {
        fprintf(stderr, "%s: Ignoring invalid value of %s\n", envchain_name,
                creds[i]->TargetName);
      } else {
        const std::string target_name = creds[i]->TargetName;
        const std::string::size_type n = target_name.find_first_of('/', TARGET_PREFIX_LEN);
        if (n != std::string::npos) {
          const std::string name = target_name.substr(TARGET_PREFIX_LEN, n - TARGET_PREFIX_LEN);
          if (names.insert(name).second) {
            callback(name.c_str(), data);
          }
        }
      }
    }
  }
  CredFree(creds);
  return 0;
}

int
envchain_search_values(const char *name, envchain_search_callback callback, void *data)
{
  const std::string filter = std::string("envchain/") + name + "/*";
  CREDENTIAL **creds = nullptr;
  DWORD count = 0;
  if (!CredEnumerate(filter.c_str(), 0, &count, &creds)) {
    const DWORD c = GetLastError();
    if (c == ERROR_NOT_FOUND) {
      return 0;
    } else {
      fprintf(stderr, "%s: CredEnumerate failed with %lu\n", envchain_name, c);
      return 1;
    }
  }

  for (DWORD i = 0; i < count; i++) {
    if (creds[i]->Type == CRED_TYPE_GENERIC) {
      if (creds[i]->CredentialBlob[creds[i]->CredentialBlobSize] != '\0') {
        fprintf(stderr, "%s: Ignoring invalid value of %s\n", envchain_name,
                creds[i]->TargetName);
      } else {
        callback(creds[i]->UserName,
                 reinterpret_cast<char *>(creds[i]->CredentialBlob), data);
      }
    }
  }
  CredFree(creds);
  return 0;
}

void
envchain_save_value(const char *name, const char *key, char *value, int require_passphrase)
{
  if (require_passphrase == 1) {
    fprintf(
        stderr,
        "%s: Sorry, `--require-passphrase' is unsupported on this platform\n",
        envchain_name);
    return;
  }

  const size_t target_name_len = TARGET_PREFIX_LEN + strlen(name) + 1 + strlen(key);
  char *target_name = new char[target_name_len + 1];
  snprintf(target_name, target_name_len + 1, "envchain/%s/%s", name, key);
  char *username = strdup(key);

  CREDENTIAL cred = {};
  cred.Type = CRED_TYPE_GENERIC;
  cred.TargetName = target_name;
  cred.UserName = username;
  cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
  cred.CredentialBlob = (LPBYTE)value;
  cred.CredentialBlobSize = strlen(value) + 1;
  const BOOL ret = CredWrite(&cred, 0);
  delete[] target_name;
  free(username);
  if (!ret) {
    const DWORD c = GetLastError();
    fprintf(stderr,
            "%s: Failed to save value of %s.%s with GetLastError() = %lu\n",
            envchain_name, name, key, c);
  }
}

char *
envchain_ask_value(const char *name, const char *key, int noecho)
{
  if (noecho) {
    fprintf(stderr,
            "%s: --noecho (-n) doesn't have any effect on this platform\n",
            envchain_name);
  }

  const size_t target_name_len = TARGET_PREFIX_LEN + strlen(name) + 1 + strlen(key);
  char *target_name = new char[target_name_len + 1];
  snprintf(target_name, target_name_len + 1, "envchain/%s/%s", name, key);
  const size_t message_text_len = 19 + target_name_len;
  char *message_text = new char[message_text_len + 1];
  snprintf(message_text, message_text_len, "Enter the value of %s", target_name);
  char *key_copy = strdup(key);
  CREDUI_INFO info = {};
  info.cbSize = sizeof(info);
  info.pszMessageText = message_text;
  info.pszCaptionText = target_name;

  char *password = static_cast<char *>(
      calloc(CREDUI_MAX_PASSWORD_LENGTH + 1, sizeof(*password)));
  BOOL save = FALSE;
  const DWORD ret = CredUIPromptForCredentials(
      &info, target_name, nullptr, 0, key_copy, strlen(key_copy) + 1, password,
      CREDUI_MAX_PASSWORD_LENGTH + 1, &save,
      CREDUI_FLAGS_ALWAYS_SHOW_UI | CREDUI_FLAGS_GENERIC_CREDENTIALS |
          CREDUI_FLAGS_KEEP_USERNAME | CREDUI_FLAGS_PERSIST);

  delete[] target_name;
  delete[] message_text;
  free(key_copy);
  if (!save) {
    free(password);
    if (ret == ERROR_CANCELLED) {
      return nullptr;
    } else {
      fprintf(stderr, "CredUIPromptForCredentials failed with %lu", ret);
      return nullptr;
    }
  }
  return password;
}
