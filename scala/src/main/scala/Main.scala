package com.ideniox

import org.scalatra._
import org.scalatra.json._
import org.json4s.{DefaultFormats, Formats}
import org.eclipse.jetty.server.Server
import org.eclipse.jetty.servlet.{ServletContextHandler, ServletHolder}
import org.eclipse.jetty.server.handler.ContextHandlerCollection
import org.eclipse.jetty.servlet.ServletContextHandler
import org.eclipse.jetty.server.{Server, ServerConnector}

object Main extends App {

  // Create an instance of MyServlet
  val servlet = new LLTPServlet

  // Initialize the Scalatra servlet
  servlet.init()

  // Create a new Jetty server on port 8080
  val server = new Server(8080)

  val connector = new ServerConnector(server)

  // Set the idle timeout for the connector (in milliseconds)
  connector.setIdleTimeout(60000 * 60 * 24) // Timeout of a day

  server.addConnector(connector)

  // Create a context handler and mount the servlet
  val context = new ServletContextHandler(ServletContextHandler.SESSIONS)
  context.setContextPath("/")
  context.addServlet(new ServletHolder(servlet), "/*")

  // Attach the context handler to the server
  server.setHandler(context)

  // Start the Jetty server
  server.start()
  server.join()
}

