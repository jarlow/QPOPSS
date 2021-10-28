import matplotlib.pyplot as plt
import numpy as np
from mpl_toolkits.mplot3d import Axes3D
from matplotlib import cm

showplots = True
saveplots = False

''' plots the overhead memory used by Delegation Space-Saving '''


def plot_overhead():
    threads = range(1, 128)
    df_us = range(1, 128)

    X, Y = np.meshgrid(threads, df_us)

    def function(T, df_u): return (
        40*(T**2)+4*(T**2)*2*df_u + 8 * T + 40*T)/1000000
    zs = np.array([function(x, y) for x, y in zip(np.ravel(X), np.ravel(Y))])
    Z = zs.reshape(X.shape)
    fig = plt.gcf()
    ax = Axes3D(fig)
    ax.plot_surface(X, Y, Z, cmap=plt.cm.viridis, linewidth=0.2)
    ax.set_xlabel("Threads", linespacing=3.2)
    ax.set_ylabel(r"$df_u$", linespacing=3.2)
    ax.set_zlabel("Overhead memory consumed (MB)", linespacing=3.2)
    ax.xaxis.labelpad = 20
    ax.yaxis.labelpad = 20
    ax.zaxis.labelpad = 20
    colmap = cm.ScalarMappable(cmap=plt.cm.viridis)
    colmap.set_array(zs)
    fig.colorbar(colmap, ax=ax, location="right", shrink=0.7, pad=0)
    name = "/home/victor/git/DelegationSketchTopK-singlequery/plots/vs_avgrel_finalVarydfsdfu.svg"
    if saveplots:
        plt.savefig(name, format="svg", dpi=4000)
    if showplots:
        plt.show()
    plt.cla()
    plt.clf()


'''Plot memory usage by Space-Saving and Delegation Space-Saving'''


def plot_memory_usage():
    threads = range(1, 128)
    skew = np.arange(1, 3.1, 0.1)
    eps = 0.000001

    X, Y = np.meshgrid(threads, skew)

    def function(T, a): return (T*32*(1/(T*eps))**(a**-1))/1000000
    zs = np.array([function(x, y) for x, y in zip(np.ravel(X), np.ravel(Y))])
    Z = zs.reshape(X.shape)

    def function_single(T, a): return (32*(1/(eps))**(a**-1))/1000000
    zs2 = np.array([function_single(x, y)
                    for x, y in zip(np.ravel(X), np.ravel(Y))])
    Z2 = zs2.reshape(X.shape)

    fig = plt.gcf()
    ax = Axes3D(fig)
    ax.plot_surface(X, Y, Z, cmap=plt.cm.viridis, linewidth=0.2)
    ax.plot_surface(X, Y, Z2, color='r', linewidth=0.2,
                    shade=False, alpha=0.25)
    ax.set_xlabel("Threads", linespacing=3.2)
    ax.set_ylabel("Skew", linespacing=3.2)
    ax.set_zlabel("Counters, memory consumed (MB)", linespacing=3.2)
    ax.xaxis.labelpad = 20
    ax.yaxis.labelpad = 20
    ax.zaxis.labelpad = 20
    colmap = cm.ScalarMappable(cmap=plt.cm.viridis)
    colmap.set_array(zs)
    fig.colorbar(colmap, ax=ax, location="right", shrink=0.7, pad=0)
    name = "/home/victor/git/DelegationSketchTopK-singlequery/plots/vs_avgrel_finalVarydfsdfu.svg"
    if saveplots:
        plt.savefig(name, format="svg", dpi=4000)
    if showplots:
        plt.gcf()
        plt.show()
    plt.cla()
    plt.clf()


plot_overhead()
plot_memory_usage()
