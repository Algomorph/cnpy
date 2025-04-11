#include"cnpy.h"
#include<complex>
#include<cstdlib>
#include<iostream>
#include<map>
#include<string>
#include <random>

const int Nx = 128;
const int Ny = 64;
const int Nz = 32;

int main()
{
    //set random seed so that result is reproducible (for testing)
    srand(0);
    //create random data
    std::vector<std::complex<double>> data(Nx*Ny*Nz);
    for(int i = 0;i < Nx*Ny*Nz;i++) data[i] = std::complex<double>(rand(),rand());

    //save it to file
    cnpy::npy_save("arr1.npy", &data[0], {Nz, Ny, Nx}, "w", false);

    //load it into a new array
    cnpy::NpyArray arr = cnpy::npy_load("arr1.npy");
    std::complex<double>* loaded_data = arr.data<std::complex<double>>();
    
    //make sure the loaded data matches the saved data
    assert(arr.word_size == sizeof(std::complex<double>));
    assert(arr.shape.size() == 3 && arr.shape[0] == Nz && arr.shape[1] == Ny && arr.shape[2] == Nx);
    assert(arr.dtype == cnpy::NPY_CDOUBLE);
    for(int i = 0; i < Nx*Ny*Nz;i++) assert(data[i] == loaded_data[i]);

    //append the same data to file
    //npy array on file now has shape (Nz+Nz,Ny,Nx)
    cnpy::npy_save("arr1.npy", &data[0], {Nz, Ny, Nx}, "a", false);

    //now write to an npz file
    //non-array variables are treated as 1D arrays with 1 element
    double myVar1 = 1.2;
    char myVar2 = 'a';
    cnpy::npz_save("out.npz","myVar1",&myVar1,{1},"w"); //"w" overwrites any existing file
    cnpy::npz_save("out.npz","myVar2",&myVar2,{1},"a"); //"a" appends to the file we created above
    cnpy::npz_save("out.npz","arr1",&data[0],{Nz,Ny,Nx},"a"); //"a" appends to the file we created above

    //load a single var from the npz file
    cnpy::NpyArray arr2 = cnpy::npz_load("out.npz","arr1");

    //load the entire npz file
    cnpy::npz_t my_npz = cnpy::npz_load("out.npz");
    
    //check that the loaded myVar1 matches myVar1
    cnpy::NpyArray arr_mv1 = my_npz["myVar1"];
    double* mv1 = arr_mv1.data<double>();
    assert(arr_mv1.shape.size() == 1 && arr_mv1.shape[0] == 1);
    assert(mv1[0] == myVar1);
    assert(arr_mv1.dtype == cnpy::NPY_DOUBLE);

    //create random int64_t data
    std::vector<int64_t> data_int64_t(Nx*Ny*Nz);
    std::mt19937_64 random_generator(12345);
    std::uniform_int_distribution<int64_t> distribution_int64_t(std::numeric_limits<int64_t>::min(),
                                                                std::numeric_limits<int64_t>::max());
    for(int i = 0;i < Nx*Ny*Nz;i++) data_int64_t[i] = distribution_int64_t(random_generator);

    //save it to file
    cnpy::npy_save("arr_int64_t.npy", &data_int64_t[0], {Nz, Ny, Nx}, "w", false);

    //load it into a new array
    cnpy::NpyArray arr_int64_t = cnpy::npy_load("arr_int64_t.npy");
    int64_t* loaded_data_int64_t = arr_int64_t.data<int64_t>();

    //make sure the loaded data matches the saved data
    assert(arr_int64_t.word_size == sizeof(int64_t));
    assert(arr_int64_t.shape.size() == 3 && arr_int64_t.shape[0] == Nz && arr_int64_t.shape[1] == Ny && arr_int64_t.shape[2] == Nx);
    assert(arr_int64_t.dtype == cnpy::NPY_LONGLONG);
    for(int i = 0; i < Nx*Ny*Nz;i++) assert(data_int64_t[i] == loaded_data_int64_t[i]);

}
