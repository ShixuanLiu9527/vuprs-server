#include "protocol.h"

static bool __ParseDoubleFromJsonValue(double *target, const nlohmann::json &value)
{
	if (target == nullptr) return false;
	try
	{
		if (value.is_number_float() || value.is_number_integer() || value.is_number_unsigned())
		{
			*target = value.get<double>();
			return true;
		}
		if (value.is_string())
		{
			const std::string valueString = value.get<std::string>();
			size_t idx = 0;
			*target = std::stod(valueString, &idx);
			return idx == valueString.size();
		}
	}
	catch (...)
	{
		return false;
	}
	return false;
}
static std::string __ServerCommandToString(vuprs::ServerCommand cmd)
{
	switch (cmd)
	{
		case vuprs::ServerCommand::SERVER_CMD__RESET:
		{
			return "reset";
		}
		case vuprs::ServerCommand::SERVER_CMD__REDIRECT:
		{
			return "redirect";
		}
		case vuprs::ServerCommand::SERVER_CMD__CHANGE_BEAMFORMER:
		{
			return "change_beam_former";
		}
		case vuprs::ServerCommand::SERVER_CMD__CHANGE_ALG_PARAM:
		{
			return "change_algorithm_param";
		}
		case vuprs::ServerCommand::SERVER_CMD__STOP:
		{
			return "stop";
		}
		case vuprs::ServerCommand::SERVER_CMD__START:
		{
			return "start";
		}
		case vuprs::ServerCommand::SERVER_CMD__GET_NEW_DATA:
		{
			return "get_data";
		}
		default:
		{
			return "invalid";
		}
	}
}

std::string vuprs::RemoveFrameIfExists(const std::string &message, const std::string &header, const std::string &tailer)
{
    std::string result = message;

    if (!header.empty() && result.size() >= header.size() &&
        result.compare(0, header.size(), header) == 0)
    {
        result.erase(0, header.size());
    }

    if (!tailer.empty() && result.size() >= tailer.size() &&
        result.compare(result.size() - tailer.size(), tailer.size(), tailer) == 0)
    {
        result.erase(result.size() - tailer.size(), tailer.size());
    }

    return result;
}

std::string vuprs::AddFrameIfMissing(const std::string &message, const std::string &header, const std::string &tailer)
{
    std::string result = message;

    const bool hasHeader = (!header.empty() && result.size() >= header.size() &&
                            result.compare(0, header.size(), header) == 0);

    const bool hasTailer = (!tailer.empty() && result.size() >= tailer.size() &&
                            result.compare(result.size() - tailer.size(), tailer.size(), tailer) == 0);

    if (hasHeader && hasTailer)
    {
        return result;
    }

    if (!hasHeader && !header.empty())
    {
        result = header + result;
    }

    if (!hasTailer && !tailer.empty())
    {
        result += tailer;
    }

    return result;
}

bool vuprs::PROTOCOL_ParseCommandFromMessage(const std::string &message, vuprs::ServerCommandInformation *cmd)
{
	if (cmd == nullptr) return false;
	if (message.empty()) return false;

	vuprs::Set_ARM_FPGA_BF_Config_ToDefault(&cmd->config);

	try
	{
		nlohmann::json root = nlohmann::json::parse(message);

		if (!root.contains("cmd") || !root["cmd"].is_string()) return false;

		std::string command = root["cmd"].get<std::string>();

		if (command == "reset")
		{
			cmd->cmd = vuprs::ServerCommand::SERVER_CMD__RESET;
			return true;
		}

		if (command == "change_beam_former")
		{
			if (!root.contains("params") || !root["params"].is_object()) return false;
			if (!root["params"].contains("beamformer") || !root["params"]["beamformer"].is_string()) return false;

			const std::string beamformerName = root["params"]["beamformer"].get<std::string>();
			if (beamformerName != "mvdr" && beamformerName != "cbf" && beamformerName != "dcrcb") return false;

			cmd->cmd = vuprs::ServerCommand::SERVER_CMD__CHANGE_BEAMFORMER;
			cmd->beamformer_name = beamformerName;
			return true;
		}

		if (command == "redirect")
		{
			if (!root.contains("params") || !root["params"].is_object()) return false;
			if (!root["params"].contains("alt") || !root["params"].contains("az")) return false;

			double alt = 0.0;
			double az = 0.0;
			if (!__ParseDoubleFromJsonValue(&alt, root["params"]["alt"])) return false;
			if (!__ParseDoubleFromJsonValue(&az, root["params"]["az"])) return false;

			cmd->cmd = vuprs::ServerCommand::SERVER_CMD__REDIRECT;
			cmd->config.bf_target__alt = alt;
			cmd->config.bf_target__az = az;
			return true;
		}

		if (command == "start")
		{
			cmd->cmd = vuprs::ServerCommand::SERVER_CMD__START;
			return true;
		}

		if (command == "stop")
		{
			cmd->cmd = vuprs::ServerCommand::SERVER_CMD__STOP;
			return true;
		}

		if (command == "get_data")
		{
			cmd->cmd = vuprs::ServerCommand::SERVER_CMD__GET_NEW_DATA;
			return true;
		}
	}
	catch (...)
	{
		return false;
	}

	return false;
}

std::string vuprs::PROTOCOL_MakeServerResponse(const vuprs::ServerCommandInformation &cmd, bool operationStatus)
{
	nlohmann::json response;
	response["response_cmd"] = __ServerCommandToString(cmd.cmd);
	response["operation_status"] = operationStatus ? "done" : "failed";
	return response.dump();
}
