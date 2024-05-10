package com.ideniox

import org.scalatra._
import org.json4s._
import org.scalatra.json._
import org.scalatra.json.JacksonJsonSupport
import org.json4s.jackson.JsonMethods._

class LLTPServlet extends ScalatraServlet with JacksonJsonSupport {

  protected implicit lazy val jsonFormats: Formats = DefaultFormats

  before() {
    contentType = formats("json")
  }

  post("/lltp") {
    try {
      val body = parse(request.body)
      val numbers = (body \ "numbers").extract[String].split(",").map(_.toInt).toList
      val numThreads = (body \ "numThreads").extract[Int]
      Ok(LLTP.computeLLTP(numbers, numThreads))
    } catch {
      case e: Exception =>
        println(s"An error occurred: ${e.getMessage}")
        InternalServerError("An error occurred while processing the request.")
    }
  }
}
