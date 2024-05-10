organization := "com.ideniox"

name := "LLTP"

version := "0.1.0"

scalaVersion := "2.13.12"

libraryDependencies ++= Seq(
  "org.scalatra" %% "scalatra" % "2.8.0",
  "org.scalatra" %% "scalatra-json" % "2.8.0",
  "org.scalatra" %% "scalatra-scalatest" % "2.8.0",
  "org.json4s" %% "json4s-jackson" % "4.0.0",
  "org.json4s" %% "json4s-jackson" % "4.0.0",
  "org.json4s" %% "json4s-jackson" % "4.0.0",
  "javax.servlet" % "javax.servlet-api" % "3.1.0",
  "org.eclipse.jetty" % "jetty-servlet" % "10.0.0",
  "ch.qos.logback" % "logback-classic" % "1.5.6"
)

dependencyOverrides += "org.scala-lang.modules" %% "scala-xml" % "2.0.1"

assemblyMergeStrategy in assembly := {
  case "META-INF/MANIFEST.MF" => MergeStrategy.discard
  case PathList("META-INF", xs @ _*) => MergeStrategy.last
  case PathList("com", "ideniox", xs @ _*) => MergeStrategy.first
  case x => MergeStrategy.first
}


mainClass := Some("com.ideniox.Main") // Assuming package and class names

