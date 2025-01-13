    #include <opencv2/opencv.hpp>
    #include <cmath>
    #include <vector>
    #include <thread>
    #include <future>
    #include <numeric>
    #include <mpi.h>


    using namespace std;
    using namespace cv;

    static void houghSerial(const Mat &edges, vector<int> &acc, int &nr, int &nt, float dr=1.f, float dth=1.f) {
        int rows = edges.rows, cols = edges.cols;
        int d = static_cast<int>(ceil(sqrt(rows*rows + cols*cols)));
        nr = 2*d;
        nt = static_cast<int>(180.f / dth);
        acc.assign(nr*nt, 0);
        for(int y=0;y<rows;y++){
            const uchar* rowPtr = edges.ptr<uchar>(y);
            for(int x=0;x<cols;x++){
                if(rowPtr[x]){
                    for(int t=0;t<nt;t++){
                        float rad = (t*dth)*CV_PI/180.f;
                        int r = cvRound((x*cos(rad) + y*sin(rad))/dr) + d;
                        acc[r*nt + t]++;
                    }
                }
            }
        }
    }

    static void houghThreads(const Mat &edges, vector<int> &acc, int &nr, int &nt, int numThreads=4, float dr=1.f, float dth=1.f) {
        int rows = edges.rows, cols = edges.cols;
        int d = static_cast<int>(ceil(sqrt(rows*rows + cols*cols)));
        nr = 2*d;
        nt = static_cast<int>(180.f / dth);
        acc.assign(nr*nt, 0);
        vector<pair<int,int>> coords;
        coords.reserve(rows*cols);
        for(int y=0;y<rows;y++){
            const uchar* rowPtr = edges.ptr<uchar>(y);
            for(int x=0;x<cols;x++){
                if(rowPtr[x]) coords.push_back({y,x});
            }
        }
        vector<future<vector<int>>> futures;
        int chunk = max(1, (int)coords.size() / numThreads);
        auto worker = [&](int start, int end){
            vector<int> localAcc(nr*nt, 0);
            for(int i=start;i<end;i++){
                int y = coords[i].first, x = coords[i].second;
                for(int t=0;t<nt;t++){
                    float rad = (t*dth)*CV_PI/180.f;
                    int r = cvRound((x*cos(rad) + y*sin(rad))/dr) + d;
                    localAcc[r*nt + t]++;
                }
            }
            return localAcc;
        };
        for(int i=0; i<(int)coords.size(); i+=chunk){
            futures.push_back(async(std::launch::async, worker, i, min(i+chunk, (int)coords.size())));
        }
        for(auto &f : futures){
            vector<int> part = f.get();
            for(size_t i=0; i<acc.size(); i++){
                acc[i] += part[i];
            }
        }
    }

    static void houghMPI(const Mat &edges, vector<int> &acc, int &nr, int &nt, float dr=1.f, float dth=1.f) {
        int rank, size;
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        MPI_Comm_size(MPI_COMM_WORLD, &size);
        int rows = edges.rows, cols = edges.cols;
        int d = static_cast<int>(ceil(sqrt(rows*rows + cols*cols)));
        nr = 2*d;
        nt = static_cast<int>(180.f / dth);
        acc.assign(nr*nt, 0);
        vector<pair<int,int>> coords;
        if(rank == 0){
            coords.reserve(rows*cols);
            for(int y=0; y<rows; y++){
                const uchar* rowPtr = edges.ptr<uchar>(y);
                for(int x=0; x<cols; x++){
                    if(rowPtr[x]) coords.push_back({y,x});
                }
            }
        }
        int total = (int)coords.size();
        MPI_Bcast(&total, 1, MPI_INT, 0, MPI_COMM_WORLD);
        int chunk = total / size;
        int start = rank * chunk;
        int end = (rank == size-1 ? total : (rank+1)*chunk);
        vector<int> xData(total), yData(total);
        if(rank == 0){
            for(int i=0; i<total; i++){
                xData[i] = coords[i].second;
                yData[i] = coords[i].first;
            }
        }
        MPI_Bcast(xData.data(), total, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(yData.data(), total, MPI_INT, 0, MPI_COMM_WORLD);
        vector<int> localAcc(nr*nt, 0);
        for(int i=start; i<end; i++){
            int x = xData[i], y = yData[i];
            for(int t=0; t<nt; t++){
                float rad = (t*dth)*CV_PI/180.f;
                int r = cvRound((x*cos(rad) + y*sin(rad))/dr) + d;
                localAcc[r*nt + t]++;
            }
        }
        MPI_Reduce(localAcc.data(), acc.data(), nr*nt, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
    }

    int main(int argc, char** argv){
        MPI_Init(&argc,&argv);
        int rank; MPI_Comm_rank(MPI_COMM_WORLD,&rank);

        Mat img = imread("test.png", IMREAD_GRAYSCALE);
        if(img.empty()){
            if(rank==0) cerr << "Cannot read test.png" << endl;
            MPI_Finalize();
            return -1;
        }
        Mat edges; Canny(img, edges, 50, 150);

        vector<int> acc; int nr, nt;
        houghSerial(edges, acc, nr, nt);
        houghThreads(edges, acc, nr, nt);
        houghMPI(edges, acc, nr, nt);


        if(rank == 0){
            Mat color; cvtColor(img, color, COLOR_GRAY2BGR);
            float dr = 1.f, dth = 1.f;
            int d = nr/2;
            for(int r=0; r<nr; r++){
                for(int t=0; t<nt; t++){
                    if(acc[r*nt + t] > 100){
                        float theta = t*dth*CV_PI/180.f;
                        float rho = (r - d)*dr;
                        double a = cos(theta), b = sin(theta);
                        double x0 = a*rho, y0 = b*rho;
                        Point pt1, pt2;
                        pt1.x = cvRound(x0 + 1000*(-b));
                        pt1.y = cvRound(y0 + 1000*(a));
                        pt2.x = cvRound(x0 - 1000*(-b));
                        pt2.y = cvRound(y0 - 1000*(a));
                        line(color, pt1, pt2, Scalar(0, 0, 255), 1, LINE_AA);
                    }
                }
            }
            imwrite("hough_result.png", color);
            cout << "Result saved as hough_result.png" << endl;
        }
        MPI_Finalize();
        return 0;
    }
