/* Copyright (C) 2005-2025, UNIGINE. All rights reserved.
*
* This file is a part of the UNIGINE 2 SDK.
*
* Your use and / or redistribution of this software in source and / or
* binary form, with or without modification, is subject to: (i) your
* ongoing acceptance of and compliance with the terms and conditions of
* the UNIGINE License Agreement; and (ii) your inclusion of this notice
* in any version of this software that you use or redistribute.
* A copy of the UNIGINE License Agreement is available by contacting
* UNIGINE. at http://unigine.com/
*/


#ifndef __MIGRATION_CSHARP_H__
#define __MIGRATION_CSHARP_H__

namespace CSharp
{
	void processPath(string data_path)
	{
		string files[0] = ();
		findFilesByExt(files, data_path, ".cs");
		foreach(string file; files)
		{
			if(!skipFile(data_path, file))
				processFile(pathname(data_path), pathname(file) + basename(file));
		}
	}

	void processFile(string data_path, string file_path)
	{
		File file = new File();
		if (!file.open(file_path, "rb"))
		{
			Log::error("failed to load \"%s\"", file_path);
			delete file;
			return;
		}

		string content = file.gets();
		file.close();

		Log::info("upgrading \"%s\"", file_path);
		string property_guid = "";
		string new_content = Process::csharp(content, property_guid);
		if (new_content != content)
		{
			if (file.open(file_path, "wb"))
			{
				file.puts(new_content);
				file.close();

				string meta_file_path = file_path + ".meta";

				Xml xml = new Xml();
				xml.load(meta_file_path, 1);	// It may not exist or be wrong

				string runtime_name = withoutExtension(basename(file_path)) + ".prop";

				if (Process::csharpMeta(xml, property_guid, runtime_name))
				{
					if (xml.save(meta_file_path))
						Log::info("upgrading \"%s\"", meta_file_path);
					else
						Log::error("failed to write \"%s\"", meta_file_path);
				}

				delete xml;
			}
			else
			{
				Log::error("failed to write \"%s\"", file_path);
			}
		}

		delete file;
	}

} /* namespace CSharp */

#endif /* __MIGRATION_CSHARP_H__ */
