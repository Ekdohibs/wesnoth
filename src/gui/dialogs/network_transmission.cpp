/*
   Copyright (C) 2011 - 2016 Sergey Popov <loonycyborg@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "gui/dialogs/network_transmission.hpp"

#include "formula/string_utils.hpp"
#include "gettext.hpp"
#include "gui/auxiliary/find_widget.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/progress_bar.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "log.hpp"
#include "serialization/string_utils.hpp"
#include "video.hpp"
#include "wesnothd_connection.hpp"

namespace gui2
{

REGISTER_DIALOG(network_transmission)

void tnetwork_transmission::pump_monitor::process(events::pump_info&)
{
	if(!window_)
		return;
	connection_->poll();
	if(connection_->finished()) {
		window_.get().set_retval(twindow::OK);
	} else {
		size_t completed, total;
			completed = connection_->current();
			total = connection_->total();
		if(total) {
			find_widget<tprogress_bar>(&(window_.get()), "progress", false)
					.set_percentage((completed * 100.) / total);

			std::stringstream ss;
			ss << utils::si_string(completed, true, _("unit_byte^B")) << "/"
			   << utils::si_string(total, true, _("unit_byte^B"));

			find_widget<tlabel>(&(window_.get()), "numeric_progress", false)
					.set_label(ss.str());
			window_->invalidate_layout();
		}
	}
}

tnetwork_transmission::tnetwork_transmission(
		connection_data& connection,
		const std::string& title,
		const std::string& subtitle)
	: connection_(&connection)
	, pump_monitor_(connection_)
	, subtitle_(subtitle)
{
	register_label("title", true, title, false);
	set_restore(true);
}

void tnetwork_transmission::set_subtitle(const std::string& subtitle)
{
	subtitle_ = subtitle;
}

void tnetwork_transmission::pre_show(twindow& window)
{
	// ***** ***** ***** ***** Set up the widgets ***** ***** ***** *****
	if(!subtitle_.empty()) {
		tlabel& subtitle_label
				= find_widget<tlabel>(&window, "subtitle", false);
		subtitle_label.set_label(subtitle_);
		subtitle_label.set_use_markup(true);
	}

	pump_monitor_.window_ = window;
}

void tnetwork_transmission::post_show(twindow& /*window*/)
{
	pump_monitor_.window_.reset();
	connection_->cancel();
}

void tnetwork_transmission::wesnothd_dialog(CVideo& video, gui2::tnetwork_transmission::connection_data& conn, const std::string& msg1, const std::string& msg2)
{
	if (video.faked()) {
		while (!conn.finished()) {
			conn.poll();
			SDL_Delay(1);
		}
	}
	else {
		gui2::tnetwork_transmission(conn, msg1, msg2).show(video);
	}
}

struct read_wesnothd_connection_data : public gui2::tnetwork_transmission::connection_data
{
	read_wesnothd_connection_data(twesnothd_connection& conn) : conn_(conn) {}
	size_t total() override { return conn_.bytes_to_read(); }
	virtual size_t current()  override { return conn_.bytes_read(); }
	virtual bool finished() override { return conn_.has_data_received(); }
	virtual void cancel() override { }
	virtual void poll() override { conn_.poll(); }
	twesnothd_connection& conn_;
};

bool tnetwork_transmission::wesnothd_receive_dialog(CVideo& video, const std::string& msg, config& cfg, twesnothd_connection& wesnothd_connection)
{
	read_wesnothd_connection_data gui_data(wesnothd_connection);
	wesnothd_dialog(video, gui_data, msg, _("Waiting"));
	return wesnothd_connection.receive_data(cfg);
}

struct connect_wesnothd_connection_data : public gui2::tnetwork_transmission::connection_data
{
	connect_wesnothd_connection_data(twesnothd_connection& conn) : conn_(conn) {}
	virtual bool finished() override { return conn_.handshake_finished(); }
	virtual void cancel() override { }
	virtual void poll() override { conn_.poll(); }
	twesnothd_connection& conn_;
};

std::unique_ptr<twesnothd_connection> tnetwork_transmission::wesnothd_connect_dialog(CVideo& video, const std::string& msg, const std::string& hostname, int port)
{
	std::unique_ptr<twesnothd_connection> res(new twesnothd_connection(hostname, std::to_string(port)));
	connect_wesnothd_connection_data gui_data(*res);
	wesnothd_dialog(video, gui_data, msg, _("Connecting"));
	return res;
}


} // namespace gui2
