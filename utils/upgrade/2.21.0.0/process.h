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

#ifndef __MIGRATION_PROCESS_H__
#define __MIGRATION_PROCESS_H__

#include <utils.h>

namespace Process
{
	/******************************************************************************\
	*
	* Path
	*
	\******************************************************************************/
	void processPath(string data_path)
	{
	}

	/******************************************************************************\
	*
	* Materials
	*
	\******************************************************************************/
	void material(Material m)
	{
		string base_material = m.getBaseMaterial();

		if (base_material == "b7c07dfd4ce80edac55fcbfdd0befa02ccd20b88") base_material = "Unigine::particles_base";

		if (base_material == "Unigine::particles_base")
		{
			m.renameState("use_voxel_probe", "voxel_probe");
		}
	}

	/******************************************************************************\
	*
	* MaterialGraphs
	*
	\******************************************************************************/
	void materialGraph(MaterialGraph graph)
	{
		foreach(MaterialGraphNode node; graph.nodes)
		{
			materialNode(node);
		}
	}

	void materialNode(MaterialGraphNode node)
	{
	}

	/******************************************************************************\
	*
	* Property
	*
	\******************************************************************************/
	// local changes of the property file
	void propertyFile(Xml xml)
	{
	}

	// global changes that related to another properties
	void property(Properties::Property p)
	{
	}

	/******************************************************************************\
	*
	* Nodes
	*
	\******************************************************************************/
	void node(Xml xml)
	{
		int type_arg_id = xml.findArg("type");
		if (type_arg_id == -1)
			return;

		string node_type = xml.getArgValue(type_arg_id);
		if (node_type == "ObjectMeshSkinned")
		{
			xml.setArg("type", "ObjectMeshSkinnedLegacy");

			int mesh_name_id = xml.findChild("mesh_name");
			if (mesh_name_id != -1)
			{
				Xml mesh_name_xml = xml.getChild(mesh_name_id);
				string mesh_name = mesh_name_xml.getData();
				if (!startsWith(mesh_name, "guid://"))
					mesh_name_xml.setData(mesh_name + "_skinned");
			}

			int body_id = xml.findChild("body");
			if (body_id != -1)
			{
				Xml body_xml = xml.getChild(body_id);
				int body_type_arg_id = body_xml.findArg("type");
				if (body_type_arg_id != -1 && body_xml.getArgValue(body_type_arg_id) == "BodyRagdoll")
				{
					int id = 0;
					while (id < body_xml.getNumChildren())
					{
						Xml child_xml = body_xml.getChild(id);
						if (child_xml.getName() == "bone")
						{
							int bone_id = child_xml.findChild("bone");
							if (bone_id != -1)
							{
								Xml bone_xml = child_xml.getChild(bone_id);
								bone_xml.setData("-1");
							}
						}
						id++;
					}
				}
			}
		}

		if (node_type == "WorldTransformBone")
		{
			xml.setArg("type", "WorldTransformJoint");

			int bone_id = xml.findChild("bone");
			if (bone_id != -1)
			{
				Xml bone_xml = xml.getChild(bone_id);
				bone_xml.setName("joint");
			}
		}

		if (node_type == "ObjectWaterGlobal")
			processWaterGlobalNodeRoot(xml, xml.getParent());
	}

	void processWaterGlobalNodeRoot(Xml xml, Xml parent)
	{
		if (xml.findChild("node") == -1)
			return;

		Xml xml_water = xml;
		string water_transform = xml_water.getChildData("transform","");

		Xml xml_node_transform = new Xml();
		xml_node_transform.setName("node");
		xml_node_transform.setArg("type","NodeDummy");
		xml_node_transform.setChildData("transform", water_transform);

		for(int j = xml_water.getNumChildren() - 1; j >= 0; j--)
		{
			Xml child = xml_water.getChild(j);
			if(child.getName() == "node")
			{
				xml_node_transform.addChild("node").copy(child);
				delete xml_water.removeChild(child);
			}
		}

		// new root
		if (parent.getName() != "node")
		{
			Xml xml_new_root = parent.addChild("node");
			xml_new_root.setArg("type","NodeDummy");
			xml_new_root.setChildData("transform", mat4_identity);

			xml_new_root.addChild("node").copy(xml_node_transform);
			delete xml_node_transform;

			xml_new_root.addChild("node").copy(xml_water);
			delete parent.removeChild(xml_water);

		} else
		{
			parent.addChild("node").copy(xml_node_transform);
			delete xml_node_transform;
		}
	}

	void surfaceProperty(Xml xml_surface, Xml xml_prop, Properties::Property prop)
	{
	}

	/******************************************************************************\
	*
	* Bodies
	*
	\******************************************************************************/
	void body(Xml xml)
	{
	}

	/******************************************************************************\
	*
	* Shapes
	*
	\******************************************************************************/
	void shape(Xml xml)
	{
	}

	/******************************************************************************\
	*
	* Joints
	*
	\******************************************************************************/
	void joint(Xml xml)
	{
	}

	/******************************************************************************\
	*
	* Render
	*
	\******************************************************************************/
	void render(Xml xml)
	{
		xmlRenameChild(xml, "stereo_distance", "render_stereo_distance");
		xmlRenameChild(xml, "stereo_radius", "render_stereo_radius");
		xmlRenameChild(xml, "stereo_offset", "render_stereo_offset");

		xmlDeleteChild(xml, "render_taa_information_lost_frame_count");
		xmlDeleteChild(xml, "render_taa_max_frames_by_velocity");
		xmlDeleteChild(xml, "render_taa_min_frames_by_velocity");
		xmlDeleteChild(xml, "render_sssss_taa_max_frames_by_velocity");
		xmlDeleteChild(xml, "render_sssss_taa_min_frames_by_velocity");

		int num = xml.findChild("render_upscale_post");
		if (num != -1)
		{
			Xml child = xml.getChild(num);
			int value = child.getBoolData();

			child.setName("render_upscale_order");
			xmlSetChildValue(xml, "render_upscale_order", value ? 3 : 0);
		}

		Xml xml_new = new Xml();
		string path_render = pathname(replace(__FILE__, "\\", "/")) + "render.xml";
		if(xml_new.load(path_render))
		{
			for(int i = 0; i < xml.getNumChildren(); i++)
			{
				Xml x = xml.getChild(i);

				if (xml_new.findChild(x.getName()) != -1)
					continue;
				delete xml.removeChild(x);
				i--;
			}

			xmlMergeChildren(xml_new, xml);
		}
		delete xml_new;
	}

	/******************************************************************************\
	*
	* Sound
	*
	\******************************************************************************/
	void sound(Xml xml)
	{
	}

	/******************************************************************************\
	*
	* Physics
	*
	\******************************************************************************/
	void physics(Xml xml)
	{
	}

	/******************************************************************************\
	*
	* World
	*
	\******************************************************************************/
	void world(Xml xml)
	{
		Xml sound_xml = xmlGetChild(xml, "sound");
		if (sound_xml != NULL)
		{
			xmlDeleteChild(sound_xml, "enabled");
			if (sound_xml.getNumChildren() == 0)
				delete xml.removeChild(sound_xml);
		}
		Xml game_xml = xmlGetChild(xml, "game");
		if (game_xml != NULL)
		{
			xmlDeleteChild(game_xml, "enabled");
			if (game_xml.getNumChildren() == 0)
				delete xml.removeChild(game_xml);
		}
	}

	/******************************************************************************\
	*
	* Track
	*
	\******************************************************************************/
	void track(Xml xml)
	{
	}

	/******************************************************************************\
	*
	* Meta
	*
	\******************************************************************************/
	void meta(Xml xml, string file_name)
	{
		Xml xasset_type = xmlGetChild(xml, "type");
		if (xasset_type != NULL)
		{
			if (xasset_type.getData() == "fbx")
			{
				Xml xparameters = xmlGetChild(xml, "parameters");
				if (xparameters != NULL)
				{
					int num_children = xparameters.getNumChildren();
					for (int i = 0; i < num_children; i++)
					{
						Xml xchild = xparameters.getChild(i);
						if (xchild.getName() != "parameter")
							continue;
						if (xchild.getArg("name") != "need_triagulate")
							continue;
						xchild.setArg("name", "need_triangulate");
						break;
					}
				}
			}
			else if (xasset_type.getData() == "lmap")
			{
				enum
				{
					LANDSCAPE_TYPE_DATA_HEIGHT = 0,
					LANDSCAPE_TYPE_DATA_ALBEDO,

					LANDSCAPE_TYPE_DATA_MASK_0,
				};

				string lmap_filepath = extension(file_name, "");
				File file = new File();
				if (!file.open(lmap_filepath, "rb"))
				{
					Log::error("can't open lmap file by filepath(%s).\n", lmap_filepath);
					delete file;
					return;
				}

				file.readInt(); // skip magic
				file.readInt(); file.readInt(); //skip size
				file.readInt(); file.readInt(); //skip resolution

				int enabled = file.readInt();
				int enabled_opacity = file.readInt();

				delete file;

				int data_to_enum[] = (
					"heightmap_data" : LANDSCAPE_TYPE_DATA_HEIGHT,
					"albedo_data" : LANDSCAPE_TYPE_DATA_ALBEDO,
				);

				for (int i = 0; i < 20; i++)
				{
					string mask_data_name = "mask_data_" + string(i);
					data_to_enum[mask_data_name] = LANDSCAPE_TYPE_DATA_MASK_0 + i;
				}

				Xml xparameters = xmlGetChild(xml, "parameters");
				if (xparameters != NULL)
				{
					int num_children = xparameters.getNumChildren();
					for (int i = 0; i < num_children; i++)
					{
						Xml xchild = xparameters.getChild(i);
						if (xchild.getName() != "parameter")
							continue;

						string parameter_name = xchild.getArg("name");
						if (data_to_enum.check(parameter_name))
						{
							int enum_id = data_to_enum[parameter_name];
							int enabled_mask = (1 << enum_id);
							int is_data_enabled = (enabled & enabled_mask) != 0;
							int is_data_opacity_enabled = (enabled_opacity & enabled_mask) != 0;
							xchild.setData(is_data_enabled + is_data_opacity_enabled);
						}
					}
				}
			}
		}
	}

	/******************************************************************************\
	*
	* AppProjection
	*
	\******************************************************************************/
	void projectionBinary(string file_name)
	{
	}

	void projection(Xml xml)
	{
	}

	/******************************************************************************\
	*
	* Syncker
	*
	\******************************************************************************/
	void synckerBinary(string file_name)
	{
	}

	void syncker(Xml xml)
	{
	}

	/******************************************************************************\
	*
	* Configs
	*
	\******************************************************************************/
	void configBoot(Xml xml)
	{
	}

	void configControls(Xml xml)
	{
	}

	void configUser(Xml xml)
	{
	}

	/******************************************************************************\
	*
	* Editor's hotkey preset
	*
	\******************************************************************************/
	void editorHotkeyPreset(Json json)
	{
	}

	/******************************************************************************\
	*
	* Umounts
	*
	\******************************************************************************/
	void umount(Json json)
	{
	}

	/******************************************************************************\
	*
	* Anims
	*
	\******************************************************************************/
	void anim(string file)
	{
		migration.migrate_anim_2_21_0_0(file);
	}

	/******************************************************************************\
	*
	* Meshes
	*
	\******************************************************************************/
	void mesh(string file)
	{
		migration.migrate_mesh_2_21_0_0(file);
	}

	/******************************************************************************\
	*
	* C# files
	*
	\******************************************************************************/
	int is_space_tab(int c)
	{
		return c == ' ' || c == '\t';
	}

	int is_word_char(int c)
	{
		return isAlphaNum(c) || c == '_';
	}

	int match_literal(string s, int &pos, string lit)
	{
		int n = strlen(lit);
		if (pos + n > strlen(s))
			return 0;
		if (substr(s, pos, n) != lit)
			return 0;
		pos += n;
		return 1;
	}

	void skip_spaces_tabs(string s, int &pos)
	{
		int len = strlen(s);
		while (pos < len && is_space_tab(s[pos]))
			pos++;
	}

	// Checks whole-word occurrence inside [from, to)
	int contains_word(string s, string word, int from, int to)
	{
		int wn = strlen(word);
		if (wn == 0)
			return 0;
		int slen = strlen(s);
		if (to > slen)
			to = slen;

		for (int i = from; i + wn <= to; i++)
		{
			if (substr(s, i, wn) != word)
				continue;

			int before_ok = 1;
			int after_ok  = 1;

			if (i > from)
				before_ok = !is_word_char(s[i - 1]);
			if (i + wn < to)
				after_ok = !is_word_char(s[i + wn]);
			if (before_ok && after_ok)
				return 1;
		}
		return 0;
	}

	// Returns position of whole word inside [from, to), or -1
	int find_word_pos(string s, string word, int from, int to)
	{
		int wn = strlen(word);
		if (wn == 0)
			return -1;
		int slen = strlen(s);
		if (to > slen)
			to = slen;

		for (int i = from; i + wn <= to; i++)
		{
			if (substr(s, i, wn) != word)
				continue;

			int before_ok = 1;
			int after_ok  = 1;

			if (i > from)
				before_ok = !is_word_char(s[i - 1]);
			if (i + wn < to)
				after_ok = !is_word_char(s[i + wn]);
			if (before_ok && after_ok)
				return i;
		}
		return -1;
	}

	/******************************************************************************\
	*
	* Sandworm
	*
	\******************************************************************************/

	void sworm(Json json)
	{
		jsonSetString(json, "version", ENGINE_VERSION);

		Json objects = json.getChild("objects");
		if (objects == NULL)
		{
			return;
		}

		for (int i = 0; i < objects.getNumChildren(); i++)
		{
			Json obj = objects.getChild(i);
			Json json_type = obj.getChild("type");
			if (json_type == NULL)
			{
				Log::error("failed to find \"type\"");
				continue;
			}

			string object_type = json_type.getString();
			if (strncmp(object_type, "ObjectPoint", 11))
			{
				continue;
			}

			Json is_fence = obj.find("is_fence");
			if (is_fence == NULL)
				continue;

			if (is_fence.getBool())
			{
				jsonSetString(obj, "type", "ObjectFence");

				Json sbl = obj.getChild("split_billboards_length");
				if (sbl == NULL)
				{
					Log::error("failed to find \"split_billboards_length\"");
					continue;
				}
				sbl.setName("split_length");

				Json skew = obj.addChild("skew");
				skew.setBool(false);

				obj.addChild("max_visibility", 100);
				obj.addChild("max_fade", 0);
			}
			else
			{
				obj.removeChild("auto_size");
				obj.removeChild("step");
				obj.removeChild("start_offset");
				obj.removeChild("end_offset");
				obj.removeChild("fit_to_size");
			}
			obj.removeChild("is_fence");
		}
	}

	// Find end-of-line index (position after '\n' if present; otherwise end of string)
	int find_line_end(string s, int start)
	{
		int n = strlen(s);
		int i = start;
		while (i < n && s[i] != '\n')
			i++;
		if (i < n && s[i] == '\n')
			i++; // include '\n'
		return i;
	}

	// Matches line like:
	// [Component(PropertyGuid = "40_alnum_chars")]
	// with arbitrary spaces/tabs, and optional trailing \r before \n.
	int match_component_propertyguid_line(string line, string &out_property_guid)
	{
		int n = strlen(line);
		int pos = 0;

		skip_spaces_tabs(line, pos);

		if (pos >= n || line[pos] != '[')
			return 0;
		pos++;
		skip_spaces_tabs(line, pos);

		if (!match_literal(line, pos, "Component"))
			return 0;
		skip_spaces_tabs(line, pos);

		if (pos >= n || line[pos] != '(')
			return 0;
		pos++;
		skip_spaces_tabs(line, pos);

		if (!match_literal(line, pos, "PropertyGuid"))
			return 0;
		skip_spaces_tabs(line, pos);

		if (pos >= n || line[pos] != '=')
			return 0;
		pos++;
		skip_spaces_tabs(line, pos);

		if (pos >= n || line[pos] != '"')
			return 0;
		pos++;

		// exactly 40 alnum
		for (int i = 0; i < 40; i++)
		{
			if (pos + i >= n)
				return 0;
			if (!isAlphaNum(line[pos + i]))
				return 0;
		}
		int property_guid_start_pos = pos;
		pos += 40;

		if (pos >= n || line[pos] != '"')
			return 0;
		pos++;
		skip_spaces_tabs(line, pos);

		if (pos >= n || line[pos] != ')')
			return 0;
		pos++;
		skip_spaces_tabs(line, pos);

		if (pos >= n || line[pos] != ']')
			return 0;
		pos++;
		skip_spaces_tabs(line, pos);

		// Allow optional '\r' before '\n' or end
		if (pos < n && line[pos] == '\r')
			pos++;
		// Now must be end (we pass a single line without '\n' or with it removed)
		if (pos != n)
			return 0;

		out_property_guid = substr(line, property_guid_start_pos, 40);
		return 1;
	}

	int remove_component_attr_and_add_partial(string &text, string &out_property_guid)
	{
		int n = strlen(text);

		// 1) find attribute line
		int attr_start = -1;
		int attr_end = -1;

		string property_guid = "";

		for (int i = 0; i < n; )
		{
			int e = find_line_end(text, i);
			// strip trailing '\n' for parsing + optional '\r'
			string line = substr(text, i, e - i);
			int line_length = strlen(line);
			if (line_length > 0 && line[line_length - 1] == '\n')
			{
				line = substr(line, 0, line_length - 1);
				line_length = strlen(line);
			}
			if (line_length > 0 && line[line_length - 1] == '\r')
				line = substr(line, 0, line_length - 1);

			if (match_component_propertyguid_line(line, property_guid))
			{
				attr_start = i;
				attr_end = e; // includes '\n' if present
				break;
			}

			i = e;
		}

		if (attr_start < 0)
		{
			// nothing to do
			return 1;
		}

		out_property_guid = property_guid;

		// 2) remove the attribute line
		text = substr(text, 0, attr_start) + substr(text, attr_end, n - attr_end);

		// 3) find next "public ... class" line and inject "partial " ONLY before "class"
		n = strlen(text);
		for (int i = attr_start; i <= n; )
		{
			int e = find_line_end(text, i);

			// line without '\n' and '\r' for checks
			string line = substr(text, i, e - i);
			int line_length = strlen(line);
			if (line_length > 0 && line[line_length - 1] == '\n')
			{
				line = substr(line, 0, line_length - 1);
				line_length = strlen(line);
			}
			if (line_length > 0 && line[line_length - 1] == '\r')
			{
				line = substr(line, 0, line_length - 1);
				line_length = strlen(line);
			}

			// skip leading whitespace
			int p = 0;
			skip_spaces_tabs(line, p);

			// starts with "public" (is it a standard user Component what UnigineEditor creates) ?
			if (match_literal(line, p, "public"))
			{
				// require at least one space/tab after public
				if (p < line_length && is_space_tab(line[p]))
				{
					// allow any spaces/tabs after public
					while (p < line_length && is_space_tab(line[p]))
						p++;

					// don't duplicate if line already has "partial" as a word
					if (!contains_word(line, "partial", 0, line_length))
					{
						// find "class" as a whole word and insert right before it
						int class_pos = find_word_pos(line, "class", p, line_length);
						if (class_pos >= 0)
						{
							int insert_pos_in_text = i + class_pos; // before "class"
							text = substr(text, 0, insert_pos_in_text)
								+ "partial "
								+ substr(text, insert_pos_in_text, n - insert_pos_in_text);
							return 1;
						}
					}
					else
					{
						// already partial; we're done for this transformation
						return 1;
					}
				}
			}

			if (e == i)
				break;
			i = e;
		}

		return 1;
	}

	string csharp(string content, string &out_property_guid)
	{
		string new_content = content;
		remove_component_attr_and_add_partial(new_content, out_property_guid);
		return new_content;
	}

	/******************************************************************************\
	*
	* C# meta files
	*
	\******************************************************************************/
	// returns 1 if there is any change
	int csharpMeta(Xml xml, string property_guid, string runtime_name)
	{
		int changed = 0;
		if (xml.getName() != "asset")
		{
			xml.setName("asset");
			changed = 1;
		}

		if (xml.findArg("version") == -1)
		{
			xml.setArg("version", "2.20.0.0");	// push any previous version
			changed = 1;
		}

		Xml guid_xml = xml.getChild("guid");
		if (guid_xml == NULL)
		{
			xml.addChild("guid").setData(getRandomGUID());
			changed = 1;
		}
		Xml type_xml = xml.getChild("type");
		if (type_xml == NULL)
		{
			xml.addChild("type").setData("component");
			changed = 1;
		}
		Xml hash_xml = xml.getChild("hash");
		if (hash_xml == NULL)
		{
			string hash_guid = getRandomGUID();
			xml.addChild("hash").setData(substr(hash_guid, 0, 6));
			changed = 1;
		}

		Xml runtimes_xml = xml.getChild("runtimes");
		if (runtimes_xml == NULL)
		{
			runtimes_xml = xml.addChild("runtimes");
			changed = 1;
		}

		Xml primary_runtime_xml = NULL;
		int num_runtimes = runtimes_xml.getNumChildren();
		for (int i = 0; i < num_runtimes; i++)
		{
			Xml runtime_xml = runtimes_xml.getChild(i);
			if (runtime_xml.getArg("link") != "1") // primary
				continue;
			string id = runtime_xml.getArg("id");
			if (id == property_guid)
				return changed;
			Log::warning(format("Wrong cs.meta. Guid in cs file \"%s\", guid in meta \"%s\". Stored guid from cs file to meta.", property_guid, id));
			primary_runtime_xml = runtime_xml;
			break;
		}

		if (primary_runtime_xml == NULL)
			primary_runtime_xml = runtimes_xml.addChild("runtime");
		primary_runtime_xml.setArg("id", property_guid);
		if (xml.findArg("name") == -1)
			primary_runtime_xml.setArg("name", runtime_name);
		primary_runtime_xml.setArg("link", "1");
		return 1;
	}

	/******************************************************************************\
	*
	* BuildTool config
	*
	\******************************************************************************/
	Json move_json_child(Json jold_root, Json jnew_root, string old_name, string new_name)
	{
		Json jchild = jold_root.getChild(old_name);
		if (jchild == NULL)
			return NULL;

		jchild = jold_root.removeChild(jchild);

		Json jcopy = jnew_root.addChild(new_name);
		jcopy.copy(jchild);
		jcopy.setName(new_name);

		delete jchild;

		return jcopy;
	}

	void buildToolConfig(Json json)
	{
		jsonSetString(json, "version", ENGINE_VERSION);

		{
			Json jcommon = json.addChild("common");
			move_json_child(json, jcommon, "icon", "application_icon");
			move_json_child(json, jcommon, "banner", "banner_image");
			move_json_child(json, jcommon, "binary", "binary");
			move_json_child(json, jcommon, "config", "build_configuration");
			move_json_child(json, jcommon, "os", "os");
			move_json_child(json, jcommon, "path", "output_path");
			move_json_child(json, jcommon, "precision", "precision");
			{
				Json jprecompile = json.getChild("precompile");
				if (jprecompile != NULL)
				{
					jprecompile = json.removeChild(jprecompile);
					if (jprecompile.isBool())
					{
						Json jcopy = jcommon.addChild("precompile_shaders");
						jcopy.copy(jprecompile);
						jcopy.setName("precompile_shaders");
					}
					else if (jprecompile.isString())
					{
						Json jcopy = jcommon.addChild("precompile_shaders");
						jcopy.setBool((jprecompile.getString() == "0") ? 0 : 1);
					}
					else if (jprecompile.isNumber())
					{
						Json jcopy = jcommon.addChild("precompile_shaders");
						jcopy.setBool((jprecompile.getNumber() == 0) ? 0 : 1);
					}

					delete jprecompile;
				}
			}
			move_json_child(json, jcommon, "precompile", "precompile_shaders");
			move_json_child(json, jcommon, "name", "project_name");
			move_json_child(json, jcommon, "world", "start_world");
			move_json_child(json, jcommon, "vapi", "video_api");
		}

		{
			Json jcopy = json.addChild("copy_files");
			move_json_child(json, jcopy, "rules", "rules");
		}

		{
			Json jdelete_unused = json.getChild("delete_unused");
			if (jdelete_unused == NULL)
			{
				jdelete_unused = json.addChild("delete_unused");
			}
			else
			{
				jdelete_unused = json.removeChild(jdelete_unused);

				Json jnew_delete_unused = json.addChild("delete_unused");
				Json jenabled = jnew_delete_unused.addChild("enabled");

				jenabled.copy(jdelete_unused);
				jenabled.setName("enabled");

				delete jdelete_unused;

				jdelete_unused = jnew_delete_unused;
			}

			Json jwildcards = move_json_child(json, jdelete_unused, "wildcards", "wildcards");
			if (jwildcards != NULL)
			{
				int patterns[];
				int num_children = jwildcards.getNumChildren();
				for (int i = 0; i < num_children; i++)
				{
					Json jchild = jwildcards.getChild(i);
					Json jpattern = jchild.getChild("pattern");
					if (jpattern != NULL)
					{
						patterns.append(jpattern.getString(), jpattern.getString());
					}
				}

				string new_patterns[0] = (
					"*.boot",
					"*.config",
					"*.controls",
					"*.global",
					"*.user",
				);

				foreach(string p; new_patterns)
				{
					if (patterns.check(p))
					{
						continue;
					}

					Json jchild = jwildcards.addChild("");
					jchild.addChild("pattern", p);
					jchild.addChild("primary").setBool(0);
					jchild.addChild("source").setBool(1);
				}
			}
		}

		{
			Json jlauncher = json.getChild("launcher");
			if (jlauncher == NULL)
			{
				jlauncher = json.addChild("launcher");
			}
			else
			{
				jlauncher = json.removeChild(jlauncher);

				Json jnew_launcher = json.addChild("launcher");
				Json jtype = jnew_launcher.addChild("type");

				jtype.copy(jlauncher);
				jtype.setName("type");

				delete jlauncher;

				jlauncher = jnew_launcher;
			}

			move_json_child(json, jlauncher, "args", "arguments");

			Json jold_fullscreen = json.getChild("fullscreen");
			if (jold_fullscreen != NULL)
			{
				jlauncher.addChild("full_screen", jold_fullscreen.getBool() ? "Enabled" : "Disabled");
				delete json.removeChild(jold_fullscreen);
			}
			else
			{
				move_json_child(json, jlauncher, "full_screen", "full_screen");
			}

			move_json_child(json, jlauncher, "main_debug", "main_app_cpp_debug");
			move_json_child(json, jlauncher, "main_dev", "main_app_cpp_development");
			move_json_child(json, jlauncher, "main_release", "main_app_cpp_release");

			Json jresolutions = move_json_child(json, jlauncher, "resolutions", "resolutions");
			if (jresolutions != NULL)
			{
				int num_children = jresolutions.getNumChildren();
				if (num_children > 0)
				{
					Json jchild = jresolutions.getChild(0);
					Json jold_d = jchild.getChild("d");
					if (jold_d != NULL)
					{
						Json jold_resolutions = jlauncher.removeChild(jresolutions);

						jresolutions = jlauncher.addChild("resolutions");
						jresolutions.setArray();
						int add_first_extra_resolution = 0;
						int add_second_extra_resolution = 0;
						for (int i = 0; i < num_children; i++)
						{
							Json jchild = jold_resolutions.getChild(i);
							Json jd = jchild.getChild("d");
							Json jn = jchild.getChild("n");
							Json jw = jchild.getChild("w");

							int aspect_w = jn.getInt();
							int aspect_h = jd.getInt();
							int width = jw.getInt();
							int height = width * aspect_h / aspect_w;

							jresolutions.addChild("", format("%d;%d;%d;%d", width, height, aspect_w, aspect_h));

							add_first_extra_resolution += width == 2560 && height == 1080
								&& aspect_w == 21 && aspect_h == 9;
							add_second_extra_resolution += width == 3440 && height == 1440
								&& aspect_w == 21 && aspect_h == 9;
						}

						if (add_first_extra_resolution == 0)
							jresolutions.addChild("", "2560;1080;21;9");
						if (add_second_extra_resolution == 0)
							jresolutions.addChild("", "3440;1440;21;9");

						delete jold_resolutions;
					}
				}
			}

			move_json_child(json, jlauncher, "selected_res", "resolution_selected");
			move_json_child(json, jlauncher, "vdebug", "video_debug");
			move_json_child(json, jlauncher, "vrmodes", "vr_modes");
			move_json_child(json, jlauncher, "extra_params", "user_controls");
		}

		{
			Json jpack = json.addChild("pack_data_files");
			move_json_child(json, jpack, "pack_enabled", "enabled");
			move_json_child(json, jpack, "exclude_extensions", "exclude_extensions");
			move_json_child(json, jpack, "exclude_dirs", "exclude_directories");
			move_json_child(json, jpack, "exclude_files", "exclude_files");
		}
	}
}

#endif /* __MIGRATION_PROCESS_H__ */
