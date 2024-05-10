package com.ideniox

import scala.util.{Success, Failure}
import java.util.concurrent.atomic.AtomicLong
import scala.concurrent.{Future, Await}
import scala.concurrent.ExecutionContext.Implicits.global
import scala.concurrent.duration.Duration
import scala.concurrent.{ExecutionContext}
import scala.math.BigInt
import java.util.concurrent.TimeUnit
import scala.concurrent.duration.Duration

case class LLTResult(isPrime: Boolean, p: Int)

class LLTP {

  private val completedPrimes = new AtomicLong(0)
  private val startTime = System.nanoTime()

  def computeLLT(num: Int): LLTResult = {

    if (num == 2)
      return LLTResult(true, 2)

    // Initialize BigInt s with value 4
    var s: BigInt = BigInt(4)

    // Calculate (2^num - 1) as a BigInt
    val mp: BigInt = BigInt(2).pow(num.toInt) - 1

    // Perform the loop from 3 to num
    for (i <- 3 to num) {
      // Calculate s^2
      s = (s * s - BigInt("2")) % mp
    }

    LLTResult(s % mp == BigInt("0"), num)
  }

  private def computeAndUpdateProgress(num: Int, startTime: Long, totalTasks: Int): LLTResult = {
    computeLLT(num)
  }

  def computeLLTP(numbers: List[Int], numThreads: Int = 4): List[LLTResult] = {
    val totalTasks = numbers.size
    completedPrimes.set(0) // Reset the counter

    println(s"Start checking ${numbers.length} primes up to ${numbers.last}")

    val startTime = System.nanoTime()
    val executionContext = ExecutionContext.fromExecutor(java.util.concurrent.Executors.newFixedThreadPool(numThreads))

    val futures = numbers.map { num =>
      Future {
        computeAndUpdateProgress(num, startTime, totalTasks)
      }(executionContext)
    }

    // Wait for all futures to complete
    val resultsFuture = Future.sequence(futures)
    resultsFuture.onComplete {
      case Success(results) =>
        // Process the results here
        // You can print or handle the results as needed
      case Failure(exception) =>
        println(s"\nError occurred while computing LLTP: ${exception.getMessage}")
        exception.printStackTrace() // Print the stack trace for debugging
    }

    val x = Await.result(resultsFuture, Duration.Inf)
    println("It took " + formatNanoseconds(System.nanoTime() - startTime))
    x
  }

  def formatNanoseconds(nanoseconds: Long): String = {
    val microsecond = 1000L
    val millisecond = 1000L * microsecond
    val second = 1000L * millisecond
    val minute = 60L * second
    val hour = 60L * minute

    val hours = nanoseconds / hour
    val remainingAfterHours = nanoseconds % hour
    val minutes = remainingAfterHours / minute
    val remainingAfterMinutes = remainingAfterHours % minute
    val seconds = remainingAfterMinutes / second
    val remainingAfterSeconds = remainingAfterMinutes % second
    val milliseconds = remainingAfterSeconds / millisecond
    val remainingAfterMilliseconds = remainingAfterSeconds % millisecond
    val microseconds = remainingAfterMilliseconds / microsecond
    val remainingNanoseconds = remainingAfterMilliseconds % microsecond

    if (hours > 0) s"$hours h $minutes m"
    else if (minutes > 0) s"$minutes m $seconds s"
    else if (seconds > 0) s"$seconds s $milliseconds ms"
    else if (milliseconds > 0) s"$milliseconds ms $microseconds μs"
    else if (microseconds > 0) s"$microseconds μs $remainingNanoseconds ns"
    else s"$remainingNanoseconds ns"
  }
}

object LLTP extends LLTP
