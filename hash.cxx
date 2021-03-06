// Copyright (C) Andrew Tridgell 2002 (original file)
// Copyright (C) 2006-2011 Red Hat Inc. (systemtap changes)
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "config.h"
#include "session.h"
#include "hash.h"
#include "util.h"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cerrno>

extern "C" {
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "mdfour.h"
}

using namespace std;


class hash
{
private:
  struct mdfour md4;
  std::ostringstream parm_stream;

public:
  hash() { start(); }
  hash(const hash &base) { md4 = base.md4; parm_stream << base.parm_stream.str(); }

  void start();

  void add(const unsigned char *buffer, size_t size);
  template<typename T> void add(const T& x);
  void add(const char *s) { add((const unsigned char *)s, strlen(s)); }
  void add(const std::string& s) { add((const unsigned char *)s.c_str(),
				       s.length()); }

  void add_path(const std::string& path);

  void result(std::string& r);
  std::string get_parms() { return parm_stream.str(); }
};


void
hash::start()
{
  mdfour_begin(&md4);
}


void
hash::add(const unsigned char *buffer, size_t size)
{
  parm_stream << buffer << endl;
  mdfour_update(&md4, buffer, size);
}


template <typename T> void
hash::add(const T& x)
{
  parm_stream << x << endl;
  mdfour_update(&md4, (const unsigned char *)&x, sizeof(x));
}


void
hash::add_path(const std::string& path)
{
  struct stat st;
  memset (&st, 0, sizeof(st));

  if (stat(path.c_str(), &st) != 0)
    st.st_size = st.st_mtime = -1;

  add(path);
  add(st.st_size);
  add(st.st_mtime);
}


void
hash::result(string& r)
{
  ostringstream rstream;
  unsigned char sum[16];

  mdfour_update(&md4, NULL, 0);
  mdfour_result(&md4, sum);

  for (int i=0; i<16; i++)
    {
      rstream << hex << setfill('0') << setw(2) << (unsigned)sum[i];
    }
  rstream << "_" << setw(0) << dec << (unsigned)md4.totalN;
  r = rstream.str();
}

void create_hash_log(const string &type_str, const string &parms, const string &result, const string &hash_log_path)
{
  ofstream log_file;
  time_t rawtime;
  time ( &rawtime );
  string time_str(ctime (&rawtime));

  log_file.open(hash_log_path.c_str());
  log_file << "[" << time_str.substr(0,time_str.length()-1); // erase terminated '\n'
  log_file << "] " << type_str << ":" << endl;
  log_file << parms << endl;
  log_file << _("result:") << result << endl;
  log_file.close();
}

static const hash&
get_base_hash (systemtap_session& s)
{
  if (s.base_hash)
    return *s.base_hash;

  s.base_hash = new hash();
  hash& h = *s.base_hash;

  // Hash kernel release and arch.
  h.add(s.kernel_release);
  h.add_path(s.kernel_build_tree);
  h.add(s.architecture);

  // Hash a few kernel version/build-id files too
  // (useful for kernel developers reusing a single source tree)
  h.add_path(s.kernel_build_tree + "/.config");
  h.add_path(s.kernel_build_tree + "/.version");
  h.add_path(s.kernel_build_tree + "/include/linux/compile.h");
  h.add_path(s.kernel_build_tree + "/include/linux/version.h");
  h.add_path(s.kernel_build_tree + "/include/linux/utsrelease.h");

  // Hash runtime path (that gets added in as "-R path").
  h.add_path(s.runtime_path);

  // Hash compiler path, size, and mtime.  We're just going to assume
  // we'll be using gcc. XXX: getting kbuild to spit out out would be
  // better, especially since this is fooled by ccache.
  h.add_path(find_executable("gcc"));

  // Hash the systemtap size and mtime.  We could use VERSION/DATE,
  // but when developing systemtap that doesn't work well (since you
  // can compile systemtap multiple times in 1 day).  Since we don't
  // know exactly where we're getting run from, we'll use
  // /proc/self/exe.
  // XXX well almost exactly -- valgrind throws this off
  h.add_path("/proc/self/exe");

  return h;
}


static bool
create_hashdir (systemtap_session& s, const string& result, string& hashdir)
{
  int nlevels = 1;

  // Use a N level subdir for the cache path to reduce the impact on
  // filesystems which are slow for large directories.  Let N be adjustable.
  const char *s_n = getenv("SYSTEMTAP_NLEVELS");
  if (s_n)
    {
      nlevels = atoi(s_n);
      if (nlevels < 1) nlevels = 1;
      if (nlevels > 8) nlevels = 8;
    }

  hashdir = s.cache_path;

  for (int i = 0; i < nlevels; i++)
    {
      hashdir += string("/") + result[i*2] + result[i*2 + 1];
      if (create_dir(hashdir.c_str()) != 0)
        {
          if (! s.suppress_warnings)
            cerr << _F("Warning: failed to create cache directory (\"%s\") %s, disabling cache support",
                       hashdir.c_str(), strerror(errno)) << endl;
	  s.use_cache = s.use_script_cache = false;
	  return false;
	}
    }
  return true;
}


void
find_script_hash (systemtap_session& s, const string& script)
{
  hash h(get_base_hash(s));
  struct stat st;

  // Hash getuid.  This really shouldn't be necessary (since who you
  // are doesn't change the generated output), but the hash gets used
  // as the module name.  If two different users try to run the same
  // script at the same time, we need something to differentiate the
  // module name.
  h.add(getuid());

  // Hash user-specified arguments (that change the generated module).
  h.add(s.bulk_mode);			// '-b'
  h.add(s.timing);			// '-t'
  h.add(s.prologue_searching);		// '-P'
  h.add(s.ignore_vmlinux);		// --ignore-vmlinux
  h.add(s.ignore_dwarf);		// --ignore-dwarf
  h.add(s.consult_symtab);		// --kelf, --kmap
  h.add(s.skip_badvars);		// --skip-badvars
  h.add(s.unprivileged);		// --unprivileged
  h.add(s.compatible);			// --compatible
  h.add(s.omit_werror);		        // undocumented, evil
  if (!s.kernel_symtab_path.empty())	// --kmap
    {
      h.add(s.kernel_symtab_path);
      if (stat(s.kernel_symtab_path.c_str(), &st) == 0)
        {
	  // NB: stat of /proc/kallsyms always returns size=0, mtime=now...
	  // which is a good reason to use the default /boot/System.map-2.6.xx
	  // instead.
          h.add(st.st_size);
	  h.add(st.st_mtime);
        }
    }
  for (unsigned i = 0; i < s.macros.size(); i++)
    h.add(s.macros[i]);

  // Add any custom kbuild flags (-B)
  for (unsigned i = 0; i < s.kbuildflags.size(); i++)
    h.add(s.kbuildflags[i]);

  // -d MODULE
  for (set<string>::iterator it = s.unwindsym_modules.begin();
       it != s.unwindsym_modules.end();
       it++)
    h.add_path(*it);
    // XXX: a build-id of each module might be even better

  // Add in pass 2 script output.
  h.add(script);

  // Get the directory path to store our cached script
  string result, hashdir;
  h.result(result);
  if (!create_hashdir(s, result, hashdir))
    return;

  // Update module name to be 'stap_{hash start}'.  '{hash start}'
  // must not be too long.  This shouldn't happen, since the maximum
  // size of a hash is 32 fixed chars + 1 (for the '_') + a max of 11.
  s.module_name = "stap_" + result;
  if (s.module_name.size() >= (MODULE_NAME_LEN - 1))
    s.module_name.resize(MODULE_NAME_LEN - 1);

  // 'ccache' would use a hash path of something like:
  //    s.hash_path = hashdir + "/" + result.substr(nlevels);
  // which would look like:
  //    ~/.stap_cache/A/B/CDEFGHIJKLMNOPQRSTUVWXYZABCDEF_XXX
  //
  // We're using the following so that the module can be used straight
  // from the cache if desired.  This ends up looking like this:
  //    ~/.stap_cache/A/B/stap_ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEF_XXX.ko
  s.hash_path = hashdir + "/" + s.module_name + ".ko";

  // Update C source name with new module_name.
  s.translated_source = string(s.tmpdir) + "/" + s.module_name + ".c";
  create_hash_log(string("script_hash"), h.get_parms(), result,
                  hashdir + "/" + s.module_name + "_hash.log");
}


void
find_stapconf_hash (systemtap_session& s)
{
  hash h(get_base_hash(s));

  // Add any custom kbuild flags
  for (unsigned i = 0; i < s.kbuildflags.size(); i++)
    h.add(s.kbuildflags[i]);

  // Get the directory path to store our cached stapconf parameters
  string result, hashdir;
  h.result(result);
  if (!create_hashdir(s, result, hashdir))
    return;

  s.stapconf_name = "stapconf_" + result + ".h";
  s.stapconf_path = hashdir + "/" + s.stapconf_name;
  create_hash_log(string("stapconf_hash"), h.get_parms(), result,
                  hashdir + "/stapconf_" + result + "_hash.log");
}


string
find_tracequery_hash (systemtap_session& s, const vector<string>& headers)
{
  hash h(get_base_hash(s));

  // Add the tracepoint headers to the computed hash
  for (size_t i = 0; i < headers.size(); ++i)
    h.add_path(headers[i]);

  // Add any custom kbuild flags
  for (unsigned i = 0; i < s.kbuildflags.size(); i++)
    h.add(s.kbuildflags[i]);

  // Get the directory path to store our cached module
  string result, hashdir;
  h.result(result);
  if (!create_hashdir(s, result, hashdir))
    return "";

  create_hash_log(string("tracequery_hash"), h.get_parms(), result,
                  hashdir + "/tracequery_" + result + "_hash.log");
  return hashdir + "/tracequery_" + result + ".ko";
}


string
find_typequery_hash (systemtap_session& s, const string& name)
{
  hash h(get_base_hash(s));

  // Add the typequery name to distinguish the hash
  h.add(name);

  if (name[0] == 'k')
    // Add any custom kbuild flags
    for (unsigned i = 0; i < s.kbuildflags.size(); i++)
      h.add(s.kbuildflags[i]);

  // Get the directory path to store our cached module
  string result, hashdir;
  h.result(result);
  if (!create_hashdir(s, result, hashdir))
    return "";

  create_hash_log(string("typequery_hash"), h.get_parms(), result,
                  hashdir + "/typequery_" + result + "_hash.log");
  return hashdir + "/typequery_" + result
    + (name[0] == 'k' ? ".ko" : ".so");
}


string
find_uprobes_hash (systemtap_session& s)
{
  hash h(get_base_hash(s));

  // Hash runtime uprobes paths
  h.add_path(s.runtime_path + "/uprobes");
  h.add_path(s.runtime_path + "/uprobes2");

  // Add any custom kbuild flags
  for (unsigned i = 0; i < s.kbuildflags.size(); i++)
    h.add(s.kbuildflags[i]);

  // Get the directory path to store our cached module
  string result, hashdir;
  h.result(result);
  if (!create_hashdir(s, result, hashdir))
    return "";

  create_hash_log(string("uprobes_hash"), h.get_parms(), result,
                  hashdir + "/uprobes_" + result + "_hash.log");
  return hashdir + "/uprobes_" + result;
}

/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
