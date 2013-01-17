#include <string.h>
#include <string>
#include <vector>
#include <time.h>
#include "../regal/regal_glsl.h"

#include <cstdio>
#include <cstring>
#include "dirent.h"

#ifndef _MSC_VER
#include <unistd.h>
#endif

using namespace std;


static bool ReadStringFromFile (const char* pathName, string& output) {
	FILE* file = fopen( pathName, "rb" );
	if (file == NULL)
		return false;
	fseek(file, 0, SEEK_END);
	int length = ftell(file);
	fseek(file, 0, SEEK_SET);
	if (length < 0)	{
		fclose( file );
		return false;
	}
	output.resize(length);
	int readLength = fread(&*output.begin(), 1, length, file);
	fclose(file);
	if (readLength != length)	{
		output.clear();
		return false;
	}
	return true;
}


static vector<string> GetFiles (const string& folder) {
	vector<string> res;

	#ifdef _MSC_VER
	WIN32_FIND_DATAA FindFileData;
	HANDLE hFind = FindFirstFileA ((folder+"/*-in.txt").c_str(), &FindFileData);
	if (hFind == INVALID_HANDLE_VALUE)
		return res;

	do {
		res.push_back (FindFileData.cFileName);
	} while (FindNextFileA (hFind, &FindFileData));

	FindClose (hFind);
	
	#else
	
	DIR *dirp;
	struct dirent *dp;

	if ((dirp = opendir(folder.c_str())) == NULL)
		return res;

	while ( (dp = readdir(dirp)) ) {
		string fname = dp->d_name;
		if (fname == "." || fname == "..")
			continue;
    if( fname.rfind( "-in.txt" ) == string::npos )
      continue;
		res.push_back (fname);
	}
	closedir(dirp);
	
	#endif

	return res;
}

static void DeleteFile (const string& path) {
	#ifdef _MSC_VER
	DeleteFileA (path.c_str());
	#else
	unlink (path.c_str());
	#endif
}

static bool TestFile (regal_glsl_ctx* ctx, bool vertex,	const string& testName,	const string& inputPath,	const string& hirPath,	const string& outputPath ) {
	string input;
	if (!ReadStringFromFile (inputPath.c_str(), input))	{
		printf ("\n  %s: failed to read input file\n", testName.c_str());
		return false;
	}

	bool res = true;

	regal_glsl_shader_type type = vertex ? kRegalGlslShaderVertex : kRegalGlslShaderFragment;
	regal_glsl_shader* shader = regal_glsl_parse (ctx, type, input.c_str());
  regal_glsl_add_alpha_test( shader );
  regal_glsl_gen_output( shader );

	bool parseOk = regal_glsl_get_status(shader);
	if (parseOk)	{
		string textHir = regal_glsl_get_raw_output (shader);
		string textOut = regal_glsl_get_output (shader);
		string outputHir;
		ReadStringFromFile (hirPath.c_str(), outputHir);
		string output;
		ReadStringFromFile (outputPath.c_str(), output);

    printf( "---------------------------output-begin----------------------------\n%s"
            "...........................output-end..............................\n", textOut.c_str() );
    
		if (textHir != outputHir)	{
			// write output
			FILE* f = fopen (hirPath.c_str(), "wb");
			if (!f)	{
				printf ("\n  %s: can't write to IR file!\n", testName.c_str());
			}	else {
				fwrite (textHir.c_str(), 1, textHir.size(), f);
				fclose (f);
			}
			printf ("\n  %s: does not match raw output\n", testName.c_str());
			res = false;
		}

		if (textOut != output)	{
			// write output
			FILE* f = fopen (outputPath.c_str(), "wb");
			if (!f)	{
				printf ("\n  %s: can't write to optimized file!\n", testName.c_str());
			}	else {
				fwrite (textOut.c_str(), 1, textOut.size(), f);
				fclose (f);
			}
			printf ("\n  %s: does not match optimized output\n", testName.c_str());
			res = false;
		}
	}	else {
		printf ("\n  %s: optimize error: %s\n", testName.c_str(), regal_glsl_get_log(shader));
		res = false;
	}

	regal_glsl_shader_delete (shader);

	return res;
}


int main (int argc, const char** argv) {
	if (argc < 2) {
		printf ("USAGE: regal-glsl testfolder\n");
		return 1;
	}

	regal_glsl_ctx* ctx = regal_glsl_initialize();

	string baseFolder = argv[1];

	clock_t time0 = clock();

	static const char* kTypeName[2] = { "vertex", "fragment" };
	size_t tests = 0;
	size_t errors = 0;
	for (int type = 0; type < 2; ++type) {
		string testFolder = baseFolder + "/" + kTypeName[type];

    printf ("\n** running %s tests...\n", kTypeName[type]);
    vector<string> inputFiles = GetFiles (testFolder);
    
    size_t n = inputFiles.size();
    for (size_t i = 0; i < n; ++i) {
      string inname = inputFiles[i];
      string hirname = inname.substr (0,inname.size()-strlen("-in.txt")) + "-ir.txt";
      string outname = inname.substr (0,inname.size()-strlen("-in.txt")) + "-out.txt";
      bool ok = TestFile (ctx, type==0, inname, testFolder + "/" + inname, testFolder + "/" + hirname, testFolder + "/" + outname);
      errors += ok ? 0 : 1;
      ++tests;
    }
	}
	clock_t time1 = clock();
	float timeDelta = float(time1-time0)/CLOCKS_PER_SEC;

	if (errors != 0) {
		printf ("\n**** %i tests (%.2fsec), %i !!!FAILED!!!\n", (int)tests, timeDelta, (int)errors);
	} else {
		printf ("\n**** %i tests (%.2fsec) succeeded\n", (int)tests, timeDelta);
	}
  
	regal_glsl_cleanup (ctx);

	return errors ? 1 : 0;
}
