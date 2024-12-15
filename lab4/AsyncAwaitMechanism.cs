using System;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading.Tasks;

namespace lab4
{
    public static class AsyncAwaitMechanism
    {
        public static async Task Main2(string[] args)
        {
            int downloadCount = 3;

            var downloadTasks = new List<Task>();

            for (int i = 0; i < downloadCount; i++)
            {
                downloadTasks.Add(AsyncAwaitMechanism.OneDownload());
            }

            await Task.WhenAll(downloadTasks);

        }


        public static async Task OneDownload()
        {
            try
            {
                var hostEntry = await Dns.GetHostEntryAsync(State.Host);

                var socket = new Socket(SocketType.Stream, ProtocolType.Tcp);

                var endpoint = new IPEndPoint(hostEntry.AddressList[0], State.Port);

                using (socket)
                {
                    Console.WriteLine("Connecting to server...");

                    await socket.ConnectAsync(endpoint);
                    Console.WriteLine("Connection established.");

                    var request = $"GET /~rlupsa/edu/pdp/lecture-5-futures-continuations.html HTTP/1.1\r\nHost: {State.Host}\r\n\r\n";
                    Console.WriteLine("Sending request...");

                    var requestBytes = Encoding.UTF8.GetBytes(request);
                    await socket.SendAsync(new ArraySegment<byte>(requestBytes), SocketFlags.None);
                    Console.WriteLine("Request sent. Receiving response...");

                    var response = await ReceiveAllAsync(socket);

                    Console.WriteLine("Response received:");
                    Console.WriteLine(response);
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error: {ex.Message}");
            }
        }

        private static async Task<string> ReceiveAllAsync(Socket socket)
        {
            var buffer = new byte[State.BufferLength];

            var responseBuilder = new StringBuilder();

            while (true)
            {
                var bytesReceived = await socket.ReceiveAsync(new ArraySegment<byte>(buffer), SocketFlags.None);

                if (bytesReceived > 0)
                {
                    var responseText = Encoding.UTF8.GetString(buffer, 0, bytesReceived);
                    responseBuilder.Append(responseText);
                }
                else
                {
                    break;
                }
            }

            return responseBuilder.ToString();
        }

        public sealed class State
        {
            public const string Host = "www.cs.ubbcluj.ro";
            public const int Port = 80;
            public const int BufferLength = 1024;
        }
    }
}
