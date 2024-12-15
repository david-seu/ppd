using System.Net;
using System.Net.Sockets;
using System.Text;

namespace lab4
{
    public static class TasksMechanism
    {

         public static async Task Main1(string[] args)
        {
            int downloadCount = 3;

            var downloadTasks = new List<Task>();

            for (int i = 0; i < downloadCount; i++)
            {
                downloadTasks.Add(OneDownload());
            }

            await Task.WhenAll(downloadTasks);

        }

        public static Task OneDownload()
        {
            var mainTask = Dns.GetHostEntryAsync(State.Host)
                .ContinueWith(hostTask =>
                {
                    var hostEntry = hostTask.Result;
                    var socket = new Socket(SocketType.Stream, ProtocolType.Tcp);
                    var endpoint = new IPEndPoint(hostEntry.AddressList[0], State.Port);
                    var state = new State(socket);

                    return ConnectAsync(socket, endpoint)
                        .ContinueWith(connectTask =>
                        {
                            Console.WriteLine("Connection established.");
                            var request = $"GET /~rlupsa/edu/pdp/lecture-5-futures-continuations.html HTTP/1.1\r\nHost: {State.Host}\r\n\r\n";
                            return SendAsync(socket, request);
                        })
                        .Unwrap()
                        .ContinueWith(sendTask =>
                        {
                            Console.WriteLine("Request sent. Receiving response...");
                            return ReceiveAsync(socket);
                        })
                        .Unwrap()
                        .ContinueWith(receiveTask =>
                        {
                            Console.WriteLine("Response received:");
                            Console.WriteLine(receiveTask.Result);
                            socket.Close();
                        });
                });

            return mainTask.Unwrap();
        }

        private static Task ConnectAsync(Socket socket, EndPoint endpoint)
        {
            var tcs = new TaskCompletionSource();
            socket.BeginConnect(endpoint, ar =>
            {
                try
                {
                    socket.EndConnect(ar);
                    tcs.SetResult();
                }
                catch (Exception ex)
                {
                    tcs.SetException(ex);
                }
            }, null);
            return tcs.Task;
        }

        private static Task SendAsync(Socket socket, string requestText)
        {
            var tcs = new TaskCompletionSource();
            var requestBytes = Encoding.UTF8.GetBytes(requestText);
            socket.BeginSend(requestBytes, 0, requestBytes.Length, SocketFlags.None, ar =>
            {
                try
                {
                    socket.EndSend(ar);
                    tcs.SetResult();
                }
                catch (Exception ex)
                {
                    tcs.SetException(ex);
                }
            }, null);
            return tcs.Task;
        }

        private static Task<string> ReceiveAsync(Socket socket)
        {
            var tcs = new TaskCompletionSource<string>();
            var state = new State(socket);
            Receive(socket, state, tcs);
            return tcs.Task;
        }

        private static void Receive(Socket socket, State state, TaskCompletionSource<string> tcs)
        {
            socket.BeginReceive(state.Buffer, 0, State.BufferLength, SocketFlags.None, ar =>
            {
                try
                {
                    var bytesReceived = socket.EndReceive(ar);
                    if (bytesReceived > 0)
                    {
                        var responseText = Encoding.UTF8.GetString(state.Buffer, 0, bytesReceived);
                        state.Content.Append(responseText);
                        Receive(socket, state, tcs);
                    }
                    else
                    {
                        tcs.SetResult(state.Content.ToString());
                    }
                }
                catch (Exception ex)
                {
                    tcs.SetException(ex);
                }
            }, null);
        }

        public sealed class State(Socket socket)
        {
            public const string Host = "www.cs.ubbcluj.ro";
            public const int Port = 80;
            public const int BufferLength = 1024;
            public readonly byte[] Buffer = new byte[BufferLength];
            public readonly StringBuilder Content = new();
            public readonly Socket Socket = socket;
        }
    }
}
