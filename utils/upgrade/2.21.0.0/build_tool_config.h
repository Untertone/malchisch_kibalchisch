/* Copyright (C) 2005-2026, UNIGINE. All rights reserved.
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


#ifndef __MIGRATION_BUILD_TOOL_CONFIG_H__
#define __MIGRATION_BUILD_TOOL_CONFIG_H__

namespace BuildToolConfig
{
	int find_all_files(string files[], string path, int predicate = 0) {

		int _walk_directory(string files[], string path, int predicate) {
			if(!is_dir(path)) {
				Log::error("directory \"%s\" doesn't exist", path);
				return false;
			}

			Dir dir = new Dir(path);
			if(!dir.isOpened()) {
				Log::error("failed to open directory \"%s\"", path);
				delete dir;
				return false;
			}

			forloop(int i = 0; dir.getNumFiles()) {
				string f = dir.getFileName(i);
				if(call(predicate, f)) {
					files.append(f);
				}
			}

			forloop(int i = 0; dir.getNumDirs()) {
				string d = dir.getDirName(i);
				if(!_walk_directory(files, d, predicate)) {
					dir.close();
					delete dir;
					return false;
				}
			}

			dir.close();
			delete dir;
			return true;
		}

		files.clear();

		if (predicate == 0) predicate = [](string _) { return true; };

		return _walk_directory(files, path, predicate);
	}

	void processPath(string data_path)
	{
		string old_config_path = data_path + ".editor2/.assemblytool";
		string new_config_path = data_path + ".editor2/settings/unigineeditor.buildtool.cfg";
		string new_config_dir_path = data_path + ".editor2/settings/";
		if (is_file(old_config_path) && !is_file(new_config_path) && mkdir(new_config_dir_path, 1))
		{
			int copied = copy(old_config_path, new_config_path);
			if (copied)
			{
				processFile(data_path, new_config_path);
				remove(old_config_path);
			}
		}

		// Migrate other files without any removing, renaming and so on
		string files[0] = ();
		find_all_files(files, data_path, [](string f) {
				return matchExtension(getExtension(f), ".assemblytool");
		});
		foreach(string file; files)
		{
			if(!skipFile(data_path, file))
				processFile(pathname(data_path), pathname(file) + basename(file));
		}
	}

	void processFile(string data_path, string file)
	{
		Json json = new Json();

		{
			Xml xml = new Xml();
			if (xml.load(file, 1))
			{
				migrationFromXml(xml, json);
				json.save(file);
				delete json;
				return;
			}

			delete xml;
		}

		if(!json.load(file))
		{
			Log::error("failed to load \"%s\"", file);
			delete json;
			return;
		}

		Log::info("upgrading \"%s\"", file);

		Process::buildToolConfig(json);

		json.save(file);
		delete json;
	}

	void move_xml_string(Xml xroot, Json jroot, string old_name, string new_name)
	{
		Xml xchild = xroot.getChild(old_name);
		if (xchild == NULL)
			return;

		string value = xchild.getData();
		if (strlen(value))
			jroot.addChild(new_name, value);
	}

	string get_xml_data(Xml xroot, string name)
	{
		Xml xchild = xroot.getChild(name);
		if (xchild == NULL)
			return "";
		return xchild.getData();
	}

	string binary_from_string(string v)
	{
		if (v == "dev")
			return "Development";
		if (v == "debug")
			return "Debug";
		if (v == "release")
			return "Release";
		return "";
	}

	string config_from_string(string v)
	{
		if (v == "dev")
			return "Development";
		if (v == "release")
			return "Release";
		return "";
	}

	string precision_from_string(string v)
	{
		if (v == "float")
			return "Float";
		else if (v == "double")
			return "Release";
		return "";
	}

	string vdebug_from_string(string v)
	{
		if (v == "disabled")
			return "Disabled";
		if (v == "messages")
			return "Messages";
		if (v == "asserts")
			return "Asserts";
		return "";
	}

	string vrmode_from_string(string v)
	{
		if (v == "openvr")
			return "OpenVR";
		return "";
	}

	string launcher_from_string(string v)
	{
		if (v == "binary")
			return "BinaryFile";
		if (v == "script")
			return "ScriptFile";
		return "";
	}

	void migrationFromXml(Xml xml, Json json)
	{
		if (xml.getName() != "Options")
			return;

		{
			Json jcommon = json.addChild("common");
			move_xml_string(xml, jcommon, "name", "project_name");
			move_xml_string(xml, jcommon, "path", "output_path");
			move_xml_string(xml, jcommon, "world", "start_world");
			move_xml_string(xml, jcommon, "banner", "banner_image");

			{
				string value = binary_from_string(get_xml_data(xml, "binary"));
				if (strlen(value))
					jcommon.addChild("binary", value);
			}

			{
				string value = config_from_string(get_xml_data(xml, "config"));
				if (strlen(value))
					jcommon.addChild("build_configuration", value);
			}

			{
				string value = precision_from_string(get_xml_data(xml, "precision"));
				if (strlen(value))
					jcommon.addChild("precision", value);
			}

			jcommon.addChild("video_api", "Auto");

			{
				Xml xprecompile = xml.getChild("precompile");
				if (xprecompile != NULL)
				{
					int value = xprecompile.getBoolData();
					jcommon.addChild("precompile_shaders").setBool(value);
				}
			}
		}

		{
			Json jlauncher = json.addChild("launcher");

			{
				string value = launcher_from_string(get_xml_data(xml, "launcher"));
				if (strlen(value))
					jlauncher.addChild("type", value);
			}

			{
				string value = vrmode_from_string(get_xml_data(xml, "vr"));
				if (strlen(value))
				{
					Json jvr_modes = jlauncher.addChild("vr_modes");
					jvr_modes.setArray();
					jvr_modes.addChild("", value);
				}
			}

			{
				string value = vdebug_from_string(get_xml_data(xml, "vdebug"));
				if (strlen(value))
					jlauncher.addChild("video_debug", value);
			}

			{
				Xml xfullscreen = xml.getChild("fullscreen");
				if (xfullscreen != NULL)
				{
					int value = xfullscreen.getBoolData();
					jlauncher.addChild("full_screen", value ? "Enabled" : "Disabled");
				}
			}

			move_xml_string(xml, jlauncher, "main_debug", "main_app_cpp_debug");
			move_xml_string(xml, jlauncher, "main_dev", "main_app_cpp_development");
			move_xml_string(xml, jlauncher, "main_release", "main_app_cpp_release");
		}

		{
			Xml xrules = xml.getChild("rules");
			if (xrules != NULL)
			{
				Json jcopy_files = json.addChild("copy_files");
				Json jrules = jcopy_files.addChild("rules");
				jrules.setArray();

				for (int i = 0; i < xrules.getNumChildren(); i++)
				{
					Xml xrule = xrules.getChild(i);
					Json jchild = jrules.addChild("");

					jchild.addChild("from", xrule.getArg("from"));
					jchild.addChild("to", xrule.getArg("to"));
				}
			}
		}
	}

} /* namespace BuildToolConfig */

#endif /* __MIGRATION_BUILD_TOOL_CONFIG_H__ */
