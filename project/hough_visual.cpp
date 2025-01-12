#include <opencv2/opencv.hpp>
#include <cmath>
#include <vector>
#include <thread>
#include <future>
#include <numeric>
#include <mpi.h>
#ifdef USE_OPENCL
// #include <CL/cl.h>
#endif

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

// #ifdef USE_OPENCL
// static void houghOpenCL(const Mat &edges, vector<int> &acc, int &nr, int &nt, float dr=1.f, float dth=1.f) {
//     int rows = edges.rows, cols = edges.cols;
//     int d = (int)ceil(sqrt(rows*rows + cols*cols));
//     nr = 2*d;
//     nt = (int)(180.f/dth);
//     acc.assign(nr*nt, 0);
//     cl_platform_id pid;
//     cl_device_id did;
//     cl_context ctx;
//     cl_command_queue cq;
//     cl_int err = clGetPlatformIDs(1, &pid, nullptr);
//     err = clGetDeviceIDs(pid, CL_DEVICE_TYPE_GPU, 1, &did, nullptr);
//     ctx = clCreateContext(nullptr, 1, &did, nullptr, nullptr, &err);
//     cq = clCreateCommandQueue(ctx, did, 0, &err);
//     string src="__kernel void h(__global uchar*e,int r,int c,__global int*a,int nr,int nt,int d,float dr,float dth){int idx=get_global_id(0);if(idx>=r*c)return;int y=idx/c;int x=idx%c;if(!e[idx])return;for(int t=0;t<nt;t++){float rad=(t*dth)*3.14159265359/180.0;int rr=(int)round((x*cos(rad)+y*sin(rad))/dr)+d;if(rr>=0 && rr<nr) atomic_add(&a[rr*nt+t],1);}}";
//     const char* csrc = src.c_str();
//     size_t len = src.size();
//     cl_program prog = clCreateProgramWithSource(ctx, 1, &csrc, &len, &err);
//     err = clBuildProgram(prog, 0, nullptr, nullptr, nullptr, nullptr);
//     cl_kernel k = clCreateKernel(prog, "h", &err);
//     vector<uchar> data(rows*cols);
//     for(int i=0; i<rows; i++){
//         const uchar* p = edges.ptr<uchar>(i);
//         for(int j=0; j<cols; j++){
//             data[i*cols + j] = p[j];
//         }
//     }
//     cl_mem ebuf = clCreateBuffer(ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, data.size(), data.data(), &err);
//     cl_mem abuf = clCreateBuffer(ctx, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, acc.size()*sizeof(int), acc.data(), &err);
//     err = clSetKernelArg(k, 0, sizeof(cl_mem), &ebuf);
//     err = clSetKernelArg(k, 1, sizeof(int), &rows);
//     err = clSetKernelArg(k, 2, sizeof(int), &cols);
//     err = clSetKernelArg(k, 3, sizeof(cl_mem), &abuf);
//     err = clSetKernelArg(k, 4, sizeof(int), &nr);
//     err = clSetKernelArg(k, 5, sizeof(int), &nt);
//     err = clSetKernelArg(k, 6, sizeof(int), &d);
//     err = clSetKernelArg(k, 7, sizeof(float), &dr);
//     err = clSetKernelArg(k, 8, sizeof(float), &dth);
//     size_t gsz = rows*cols;
//     err = clEnqueueNDRangeKernel(cq, k, 1, nullptr, &gsz, nullptr, 0, nullptr, nullptr);
//     err = clFinish(cq);
//     err = clEnqueueReadBuffer(cq, abuf, CL_TRUE, 0, acc.size()*sizeof(int), acc.data(), 0, nullptr, nullptr);
//     clReleaseMemObject(ebuf);
//     clReleaseMemObject(abuf);
//     clReleaseKernel(k);
//     clReleaseProgram(prog);
//     clReleaseCommandQueue(cq);
//     clReleaseContext(ctx);
// }
// #endif

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

// #ifdef USE_OPENCL
//     // If you want the final accumulator from OpenCL, call it last:
//     houghOpenCL(edges, acc, nr, nt);
// #endif

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
