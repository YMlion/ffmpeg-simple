package com.duoyi.hellojni

import android.os.Bundle
import android.support.v7.app.AppCompatActivity
import android.view.ViewGroup

open class BaseActivity : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
    }

    override fun setContentView(layoutResID: Int) {
        super.setContentView(layoutResID)

        setForegroundColor(window.decorView as ViewGroup)
    }

    open protected fun setForegroundColor(decorView: ViewGroup) {

    }
}
