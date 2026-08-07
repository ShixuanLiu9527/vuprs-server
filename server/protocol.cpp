#include "server/protocol.h"

static std::string __ServerCommandToString(vuprs::ServerCommand cmd)
{
	switch (cmd)
	{
	case vuprs::ServerCommand::SERVER_CMD__RESET:
		return SERVER_CMD__RESET__STR;
	case vuprs::ServerCommand::SERVER_CMD__REDIRECT:
		return SERVER_CMD__REDIRECT__STR;
	case vuprs::ServerCommand::SERVER_CMD__CHANGE_BEAMFORMER:
		return SERVER_CMD__CHANGE_BEAMFORMER__STR;
	case vuprs::ServerCommand::SERVER_CMD__CHANGE_ALG_PARAM:
		return SERVER_CMD__CHANGE_ALG_PARAM__STR;
	case vuprs::ServerCommand::SERVER_CMD__STOP:
		return SERVER_CMD__STOP__STR;
	case vuprs::ServerCommand::SERVER_CMD__START:
		return SERVER_CMD__START__STR;
	case vuprs::ServerCommand::SERVER_CMD__GET_BF_DATA:
		return SERVER_CMD__GET_NEW_DATA__STR;
	case vuprs::ServerCommand::SERVER_CMD__ENABLE_SCAN:
		return SERVER_CMD__ENABLE_SCAN__STR;
	case vuprs::ServerCommand::SERVER_CMD__DISABLE_SCAN:
		return SERVER_CMD__DISABLE_SCAN__STR;
	case vuprs::ServerCommand::SERVER_CMD__GET_SCAN_DATA:
		return SERVER_CMD__GET_SCAN_DATA__STR;
	case vuprs::ServerCommand::SERVER_CMD__GET_ALG_PARAM:
		return SERVER_CMD__GET_ALG_PARAM__STR;
	case vuprs::ServerCommand::SERVER_CMD__ACK:
		return SERVER_CMD__ACK__STR;
	default:
		return SERVER_CMD__INVALID__STR;
	}
}

bool vuprs::PROTOCOL_ParseCommandFromMessage(const std::string &message, vuprs::ServerCommandInformation *cmd)
{
	if (cmd == nullptr)
		return false;
	if (message.empty())
		return false;
	cmd->config.ResetMask(false);	   /* mask to false */
	cmd->scan_config.ResetMask(false); /* mask to false */
	try
	{
		nlohmann::json root = nlohmann::json::parse(message);
		if (!root.contains("cmd") || !root["cmd"].is_string())
			return false;
		std::string command = root["cmd"].get<std::string>();
		if (command == SERVER_CMD__RESET__STR)
		{
			cmd->cmd = vuprs::ServerCommand::SERVER_CMD__RESET;
			return true;
		}
		else if (command == SERVER_CMD__ACK__STR)
		{
			cmd->cmd = vuprs::ServerCommand::SERVER_CMD__ACK;
			return true;
		}
		else if (command == SERVER_CMD__CHANGE_BEAMFORMER__STR)
		{
			if (!root.contains("params") || !root["params"].is_object())
				return false;
			if (!root["params"].contains("beamformer") || !root["params"]["beamformer"].is_string())
				return false;
			const std::string beamformer_name = root["params"]["beamformer"].get<std::string>();
			if (!IS_VALID_BEAMFORMER_SELECTION(beamformer_name))
				return false;
			cmd->cmd = vuprs::ServerCommand::SERVER_CMD__CHANGE_BEAMFORMER;
			cmd->beamformer_name = beamformer_name;
			return true;
		}
		else if (command == SERVER_CMD__REDIRECT__STR)
		{
			if (!root.contains("params") || !root["params"].is_object())
				return false;
			if (!root["params"].contains("alt") || !root["params"].contains("az"))
				return false;
			double alt = 0.0, az = 0.0;
			auto params = root["params"];
			vuprs::__JsonStringParseFLOAT<double>(&alt, params, "alt", true);
			vuprs::__JsonStringParseFLOAT<double>(&az, params, "az", true);
			cmd->cmd = vuprs::ServerCommand::SERVER_CMD__REDIRECT;
			cmd->config.bf_target__alt = alt;
			cmd->config.mask.m_bf_target__alt = true;
			cmd->config.bf_target__az = az;
			cmd->config.mask.m_bf_target__az = true;
			return true;
		}
		else if (command == SERVER_CMD__ENABLE_SCAN__STR)
		{
			if (!root.contains("params") || !root["params"].is_object())
				return false;
			if (!root["params"].contains("points") || !root["params"]["points"].is_string())
				return false;
			if (!root["params"].contains("alt_min") || !root["params"]["alt_min"].is_string())
				return false;
			int points_in_hemisphere = 0;
			double alt_min = 0.0;
			auto params = root["params"];
			vuprs::__JsonStringParseINT<int>(&points_in_hemisphere, params, "points", true);
			vuprs::__JsonStringParseFLOAT<double>(&alt_min, params, "alt_min", true);
			if (alt_min < 0.0 || alt_min > 90.0 || points_in_hemisphere <= 0)
				return false;
			cmd->cmd = vuprs::ServerCommand::SERVER_CMD__ENABLE_SCAN;
			cmd->scan_config.points_in_hemisphere = points_in_hemisphere;
			cmd->scan_config.mask.m_points_in_hemisphere = true;
			cmd->scan_config.alt_min = alt_min;
			cmd->scan_config.mask.m_alt_min = true;
			return true;
		}
		else if (command == SERVER_CMD__CHANGE_ALG_PARAM__STR)
		{
			if (!root.contains("params") || !root["params"].is_object())
				return false;
			cmd->cmd = vuprs::ServerCommand::SERVER_CMD__CHANGE_ALG_PARAM;
			auto params = root["params"];
			std::vector<std::string> _cmd_strings = {"fs", "wave_velocity",
													 "lower_frequency", "upper_frequency",
													 "snapshot_window_size", "covariance_average_index"};
			std::vector<double *> _bind_target = {&cmd->config.fs,
												  &cmd->config.bf_wave_velocity,
												  &cmd->config.bf_freq__lower, &cmd->config.bf_freq__upper,
												  &cmd->config.bf_cov_snapshots_window_size, &cmd->config.bf_cov_freq_average_index};
			std::vector<bool *> _bind_target_mask = {&cmd->config.mask.m_fs,
													 &cmd->config.mask.m_bf_wave_velocity,
													 &cmd->config.mask.m_bf_freq__lower, &cmd->config.mask.m_bf_freq__upper,
													 &cmd->config.mask.m_bf_cov_snapshots_window_size, &cmd->config.mask.m_bf_cov_freq_average_index};
			int param_number = _cmd_strings.size();
			for (int i = 0; i < param_number; i++)
			{
				if (params.contains(_cmd_strings[i]))
				{
					double val = 0.0;
					vuprs::__JsonStringParseFLOAT<double>(&val, params, _cmd_strings[i], true);
					*_bind_target[i] = val;
					*_bind_target_mask[i] = true;
				}
			}
			return true;
		}
		else if (command == SERVER_CMD__GET_ALG_PARAM__STR)
		{
			cmd->cmd = vuprs::ServerCommand::SERVER_CMD__GET_ALG_PARAM;
			return true;
		}
		else if (command == SERVER_CMD__START__STR)
		{
			cmd->cmd = vuprs::ServerCommand::SERVER_CMD__START;
			return true;
		}
		else if (command == SERVER_CMD__STOP__STR)
		{
			cmd->cmd = vuprs::ServerCommand::SERVER_CMD__STOP;
			return true;
		}
		else if (command == SERVER_CMD__DISABLE_SCAN__STR)
		{
			cmd->cmd = vuprs::ServerCommand::SERVER_CMD__DISABLE_SCAN;
			return true;
		}
		else if (command == SERVER_CMD__GET_NEW_DATA__STR)
		{
			cmd->cmd = vuprs::ServerCommand::SERVER_CMD__GET_BF_DATA;
			return true;
		}
		else if (command == SERVER_CMD__GET_SCAN_DATA__STR)
		{
			cmd->cmd = vuprs::ServerCommand::SERVER_CMD__GET_SCAN_DATA;
			return true;
		}
	}
	catch (...)
	{
		return false;
	}
	return false;
}

std::string vuprs::PROTOCOL_server_response__normal(const vuprs::ServerCommandInformation &cmd, const std::string &info, bool operation_status)
{
	nlohmann::json response;
	response["response_cmd"] = __ServerCommandToString(cmd.cmd);
	response["operation-status"] = operation_status ? "done" : "failed";
	response["info"] = info;
	return response.dump();
}

std::string vuprs::PROTOCOL_server_response__get_bf_data(const std::string &info,
														 int inference_identity,
														 bool operation_status)
{
	nlohmann::json response;
	response["response_cmd"] = SERVER_CMD__GET_NEW_DATA__STR;
	response["operation-status"] = operation_status ? "done" : "failed";
	response["info"] = info;
	response["params"]["data_format"] = "uint32_t";
	response["params"]["quantization"] = "q31";
	response["params"]["inference_identity"] = std::to_string(inference_identity);
	return response.dump();
}

std::string vuprs::PROTOCOL_server_response__algo_params(const vuprs::HybridBeamformerConfig &config)
{
	nlohmann::json response;
	response["response_cmd"] = SERVER_CMD__GET_ALG_PARAM__STR;
	response["operation-status"] = "done";
	response["params"]["fs"] = std::to_string(config.fs);
	response["params"]["wave_velocity"] = std::to_string(config.bf_wave_velocity);
	response["params"]["lower_frequency"] = std::to_string(config.bf_freq__lower);
	response["params"]["upper_frequency"] = std::to_string(config.bf_freq__upper);
	response["params"]["snapshot_window_size"] = std::to_string(config.bf_cov_snapshots_window_size);
	response["params"]["covariance_average_index"] = std::to_string(config.bf_cov_freq_average_index);
	return response.dump();
}

std::string vuprs::PROTOCOL_server_response__get_scan_data(const vuprs::ScanningConfig &scan_config,
														   double min_scan_power_dB, double max_scan_power_dB,
														   const std::string &info, bool operation_status)
{
	nlohmann::json response;
	response["response_cmd"] = SERVER_CMD__GET_SCAN_DATA__STR;
	response["operation-status"] = operation_status ? "done" : "failed";
	response["info"] = info;
	response["params"]["points"] = std::to_string(scan_config.points_in_hemisphere);
	response["params"]["min_alt"] = std::to_string(scan_config.alt_min);
	response["params"]["max_power"] = std::to_string(max_scan_power_dB);
	response["params"]["min_power"] = std::to_string(min_scan_power_dB);
	response["params"]["data_format"] = "uint16_t";
	response["params"]["quantization"] = "q15";
	return response.dump();
}
