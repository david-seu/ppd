using System;
using System.Collections.Generic;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;

namespace lab4
{
    public static class DirectCallbacks
    {
        public static void Main(string[] args)
        {
            int downloadCount = 3; 

            using var countdownEvent = new CountdownEvent(downloadCount);


            for (int i = 0; i < downloadCount; i++)
            {
                int index = i + 1; 
                var thread = new Thread(() => OneDownload(countdownEvent));
                thread.Start();
            }

            countdownEvent.Wait();

        }

        public static void OneDownload(CountdownEvent countdownEvent)
        {
            try
            {
                var state = new State(new Socket(SocketType.Stream, ProtocolType.Tcp));

                var hostEntry = Dns.GetHostEntry(State.Host);
                var endpoint = new IPEndPoint(hostEntry.AddressList[0], State.Port);

                Console.WriteLine($"Initiating connection...");
                state.Socket.BeginConnect(endpoint, ar =>
                {
                    try
                    {
                        state.Socket.EndConnect(ar);
                        Console.WriteLine($"Connection established.");
                        SendRequest(state, countdownEvent);
                    }
                    catch (Exception ex)
                    {
                        Console.WriteLine($"Connection error: {ex.Message}");
                        state.SignalCompletion();
                        countdownEvent.Signal();
                    }
                }, state);
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error: {ex.Message}");
                countdownEvent.Signal(); 
            }
        }

        private static void SendRequest(State state, CountdownEvent countdownEvent)
        {
            var request = $"GET /~rlupsa/edu/pdp/lecture-5-futures-continuations.html HTTP/1.1\r\nHost: {State.Host}\r\n\r\n";
            var requestBytes = Encoding.UTF8.GetBytes(request);

            Console.WriteLine($"Sending request...");
            state.Socket.BeginSend(requestBytes, 0, requestBytes.Length, SocketFlags.None, ar =>
            {
                try
                {
                    state.Socket.EndSend(ar);
                    Console.WriteLine($"Request sent.");
                    ReceiveResponse(state, countdownEvent);
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"Send error: {ex.Message}");
                    state.SignalCompletion();
                    countdownEvent.Signal();
                }
            }, state);
        }

        private static void ReceiveResponse(State state, CountdownEvent countdownEvent)
        {
            Console.WriteLine($"Receiving response...");
            state.Socket.BeginReceive(state.Buffer, 0, State.BufferLength, SocketFlags.None, ar =>
            {
                try
                {
                    var bytesReceived = state.Socket.EndReceive(ar);

                    if (bytesReceived > 0)
                    {
                        var responseChunk = Encoding.UTF8.GetString(state.Buffer, 0, bytesReceived);
                        state.Content.Append(responseChunk);

                        ReceiveResponse(state, countdownEvent);
                    }
                    else
                    {
                        Console.WriteLine($"Response received:");
                        Console.WriteLine(state.Content.ToString());
                        state.SignalCompletion();
                        countdownEvent.Signal(); 
                    }
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"Receive error: {ex.Message}");
                    state.SignalCompletion();
                    countdownEvent.Signal();
                }
            }, state);
        }

        public sealed class State(Socket socket)
        {
            public const string Host = "www.cs.ubbcluj.ro";
            public const int Port = 80;
            public const int BufferLength = 1024;
            public readonly byte[] Buffer = new byte[BufferLength];
            public readonly StringBuilder Content = new();
            public readonly ManualResetEvent ReceiveDone = new(false);
            public readonly Socket Socket = socket;

            public void SignalCompletion() => ReceiveDone.Set();
        }
    }
}