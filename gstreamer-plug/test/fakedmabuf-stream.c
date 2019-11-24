/*
 *  fake dumbuf buffer streaming app
 *
 *  Copyright (C) Sreerenj Balachandran <sreerenjb@gnome.org>
 *
 *  Author: Sreerenj Balachandran <sreerenjb@gnome.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation; either version 2.1
 *  of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301 USA
 */

/* Compile: gcc fakedmabuf-stream.c -o fakedmabuf-stream `pkg-config --cflags --libs gstreamer-1.0` */

#include <stdio.h>
#include <string.h>
#include <gst/gst.h>

typedef struct _CustomData
{
  GstElement *pipeline;
  GMainLoop *loop;
} AppData;

static gboolean
bus_call (GstBus * bus, GstMessage * msg, gpointer data)
{
  GMainLoop *loop = (GMainLoop *) data;

  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_EOS:{
      g_print ("End-of-stream\n");
      g_main_loop_quit (loop);
      break;
    }
    case GST_MESSAGE_ERROR:{
      gchar *debug;
      GError *err;

      gst_message_parse_error (msg, &err, &debug);
      g_printerr ("Debugging info: %s\n", (debug) ? debug : "none");
      g_free (debug);

      g_print ("Error: %s\n", err->message);
      g_error_free (err);

      g_main_loop_quit (loop);

      break;
    }
    default:
      break;
  }
  return TRUE;
}

static void
send_eos_event (AppData * data)
{
  GstBus *bus;
  GstMessage *msg;

  bus = gst_element_get_bus (data->pipeline);
  gst_element_send_event (data->pipeline, gst_event_new_eos ());
  msg = gst_bus_timed_pop_filtered (bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_EOS);

  gst_message_unref (msg);
  gst_object_unref (bus);
}

/* Process keyboard input */
static gboolean
handle_keyboard (GIOChannel * source, GIOCondition cond, AppData * data)
{
  gchar *str = NULL;

  if (g_io_channel_read_line (source, &str, NULL, NULL,
          NULL) != G_IO_STATUS_NORMAL) {
    return TRUE;
  }

  switch (g_ascii_tolower (str[0])) {
    case 'q':
      send_eos_event (data);
      g_main_loop_quit (data->loop);
      break;
    default:
      break;
  }

  g_free (str);

  return TRUE;
}

int
main (int argc, char *argv[])
{
  AppData data;
  GstStateChangeReturn ret;
  GIOChannel *io_stdin;
  GError *err = NULL;
  GstBus *bus = NULL;
  guint bus_watch_id;

  /* Initialize GStreamer */
  gst_init (NULL, NULL);

  data.pipeline =
      gst_parse_launch
      ("-v fakedmabufsrc ! video/x-raw,format=BGRx,width=3840,height=1080 ! "
      "vaapipostproc format=nv12 ! vaapih264enc  tune=low-power ! "
      "filesink location=samplezz.264 --gst-plugin-path=.", &err);

  if (err) {
    g_printerr ("failed to parse pipeline: %s\n", err->message);
    g_error_free (err);
    return -1;
  }

  /* Add a keyboard watch so we get notified of keystrokes */
  io_stdin = g_io_channel_unix_new (fileno (stdin));
  g_io_add_watch (io_stdin, G_IO_IN, (GIOFunc) handle_keyboard, &data);

  /* Create a GLib Main Loop */
  data.loop = g_main_loop_new (NULL, FALSE);

  bus = gst_element_get_bus (data.pipeline);
  bus_watch_id = gst_bus_add_watch (bus, bus_call, data.loop);
  g_object_unref (bus);

  /* Start playing */
  ret = gst_element_set_state (data.pipeline, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    g_printerr ("Unable to set the pipeline to the playing state.\n");
    gst_object_unref (data.pipeline);
    return -1;
  }

  /* set  loop to run */
  g_main_loop_run (data.loop);

  /* Free resources */
  g_source_remove (bus_watch_id);
  g_main_loop_unref (data.loop);
  g_io_channel_unref (io_stdin);
  gst_element_set_state (data.pipeline, GST_STATE_NULL);

  gst_object_unref (data.pipeline);

  return 0;
}
